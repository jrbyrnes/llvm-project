#!/usr/bin/env python3
"""
Online Reinforcement Learning Training for MLGO Instruction Scheduling.

This script coordinates between LLVM (which performs instruction scheduling) and
Python (which runs the policy model) for online PPO training.

Architecture:
    +-----------------+     features      +------------------+
    |  LLVM llc       | ----------------> |  Python Server   |
    |  (scheduler)    |                   |  (policy model)  |
    |                 | <---------------- |                  |
    +-----------------+     decision      +------------------+
            |                                     |
            v                                     v
      Static Simulator                     PPO Update
      (final reward)                       (after episode)

The training loop:
1. Start inference server with current policy
2. Run llc on kernels, collecting trajectories
3. After each kernel, receive reward from Static Simulator
4. Periodically update policy using PPO
5. Repeat

Usage:
    # Basic online training
    python3 train_online.py --warmstart models/warmstart/model.pt \
        --kernels-dir /path/to/kernels/ --output models/online/

    # With custom settings
    python3 train_online.py --warmstart models/warmstart/model.pt \
        --kernels-dir /path/to/kernels/ --output models/online/ \
        --iterations 50 --samples-per-kernel 3 --port 50051
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

from inference_server import InferenceServer, SchedulingPolicy, Trajectory, Step
from inference_server import PER_INSTR_FEATURES, GLOBAL_FEATURES, MAX_NUM_CANDIDATES, NUM_OUTPUTS


# ==============================================================================
# Configuration
# ==============================================================================


@dataclass
class TrainingConfig:
    """Configuration for online RL training."""
    # Paths
    warmstart_model: Path
    kernels_dir: Path
    output_dir: Path
    llc_path: Path

    # Server settings
    host: str = "localhost"
    port: int = 50051

    # Training settings
    iterations: int = 50
    samples_per_kernel: int = 3
    ppo_epochs: int = 4
    batch_size: int = 32
    learning_rate: float = 3e-4
    clip_epsilon: float = 0.2
    entropy_coef: float = 0.01
    gamma: float = 0.99
    gae_lambda: float = 0.95

    # Exploration settings
    initial_temperature: float = 1.0
    final_temperature: float = 0.5
    temperature_decay: float = 0.98

    # Evaluation
    eval_interval: int = 5
    checkpoint_interval: int = 5

    # Device
    device: str = "cuda" if torch.cuda.is_available() else "cpu"


# ==============================================================================
# LLVM Integration
# ==============================================================================


def find_kernel_files(kernels_dir: Path) -> List[Path]:
    """Find all .ll or .mir files in directory (recursively)."""
    kernel_files = []
    for ext in ["*.ll", "*.mir"]:
        kernel_files.extend(kernels_dir.rglob(ext))  # Use rglob for recursive search
    return sorted(kernel_files)


def run_llc_with_server(
    kernel_path: Path,
    llc_path: Path,
    server_address: str,
    output_dir: Path,
    timeout: int = 300,
) -> Tuple[bool, Optional[float], str]:
    """
    Run llc with external inference server.

    Returns:
        (success, stall_ratio, output_path)
    """
    output_path = output_dir / f"{kernel_path.stem}.s"

    cmd = [
        str(llc_path),
        "-mtriple=amdgcn-amd-amdhsa",
        "-mcpu=gfx1250",
        "-amdgpu-sched-strategy=mlcoexec",
        f"--mlcoexec-inference-server={server_address}",
        "--amdgpu-enable-static-simulator=1",
        "-o", str(output_path),
        str(kernel_path),
    ]

    logger = logging.getLogger("llc")
    logger.debug(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )

        if result.returncode != 0:
            logger.error(f"llc failed for {kernel_path.name}: {result.stderr}")
            return False, None, ""

        # Parse stall ratio from Static Simulator output in assembly file
        # (not in stdout/stderr, but in the generated .s file as comments)
        stall_ratio = None
        if output_path.exists():
            try:
                with open(output_path, "r") as f:
                    asm_content = f.read()
                stall_ratio = parse_stall_ratio(asm_content)
            except Exception as e:
                logger.warning(f"Failed to read assembly for stall ratio: {e}")

        return True, stall_ratio, str(output_path)

    except subprocess.TimeoutExpired:
        logger.error(f"llc timed out for {kernel_path.name}")
        return False, None, ""
    except Exception as e:
        logger.error(f"llc error for {kernel_path.name}: {e}")
        return False, None, ""


def parse_stall_ratio(output: str) -> Optional[float]:
    """Parse stall ratio from llc output or assembly file.

    The Static Simulator outputs:
      ;;   IPC: 0.15 | Stall ratio: 80.1%
    in the assembly file as a comment.
    """
    # Look for patterns - note: assembly has "Stall ratio: XX.X%"
    patterns = [
        r"Stall ratio[:\s]+([0-9.]+)%",  # "Stall ratio: 80.1%"
        r"StallRatio[:\s=]+([0-9.]+)",
        r"stall_ratio[:\s=]+([0-9.]+)",
    ]

    for pattern in patterns:
        match = re.search(pattern, output, re.IGNORECASE)
        if match:
            value = float(match.group(1))
            # If it's a percentage (>1), convert to ratio
            if value > 1.0:
                value = value / 100.0
            return value

    return None


def send_reward_to_server(server_address: str, reward: float, kernel_name: str) -> bool:
    """Send reward signal to inference server."""
    import socket
    import json

    host, port = server_address.split(":")
    port = int(port)

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((host, port))
            sock.settimeout(5.0)

            msg = {"type": "reward", "value": reward, "kernel": kernel_name}
            sock.sendall((json.dumps(msg) + "\n").encode("utf-8"))

            response = sock.recv(1024)
            return b"ok" in response

    except Exception as e:
        logging.getLogger("reward").error(f"Failed to send reward: {e}")
        return False


# ==============================================================================
# PPO Training
# ==============================================================================


def compute_advantages(
    rewards: torch.Tensor,
    values: torch.Tensor,
    gamma: float = 0.99,
    gae_lambda: float = 0.95,
) -> Tuple[torch.Tensor, torch.Tensor]:
    """Compute GAE advantages and returns."""
    # For episodic rewards (single reward at end), we use simpler advantage
    # Since all steps share the final reward, we normalize by episode length
    advantages = rewards - values.squeeze()
    returns = rewards

    # Normalize advantages
    if len(advantages) > 1:
        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

    return advantages, returns


def trajectories_to_batch(
    trajectories: List[Trajectory],
    gamma: float = 0.99,
    device: str = "cpu",
) -> Dict[str, torch.Tensor]:
    """Convert trajectories to training batch with discounted rewards."""
    all_per_instr = []
    all_global = []
    all_actions = []
    all_log_probs = []
    all_rewards = []

    for traj in trajectories:
        if not traj.steps:
            continue

        # Discount rewards back through episode
        N = len(traj.steps)
        for i, step in enumerate(traj.steps):
            # Per-instruction features
            per_instr = np.zeros((MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES)), dtype=np.float32)
            for j, feat in enumerate(PER_INSTR_FEATURES):
                if feat in step.state:
                    values = step.state[feat]
                    per_instr[:len(values), j] = values[:MAX_NUM_CANDIDATES]

            # Global features
            global_feats = np.zeros(len(GLOBAL_FEATURES), dtype=np.float32)
            for j, feat in enumerate(GLOBAL_FEATURES):
                if feat in step.state:
                    value = step.state[feat]
                    global_feats[j] = value[0] if len(value) > 0 else 0

            all_per_instr.append(per_instr)
            all_global.append(global_feats)
            all_actions.append(step.action)
            all_log_probs.append(step.log_prob)

            # Discounted reward: later steps get more credit
            discounted_reward = traj.reward * (gamma ** (N - 1 - i))
            all_rewards.append(discounted_reward)

    if not all_per_instr:
        return {}

    return {
        "per_instr": torch.tensor(np.array(all_per_instr), dtype=torch.float32, device=device),
        "global": torch.tensor(np.array(all_global), dtype=torch.float32, device=device),
        "actions": torch.tensor(all_actions, dtype=torch.long, device=device),
        "old_log_probs": torch.tensor(all_log_probs, dtype=torch.float32, device=device),
        "rewards": torch.tensor(all_rewards, dtype=torch.float32, device=device),
    }


def ppo_update(
    policy: nn.Module,
    optimizer: optim.Optimizer,
    batch: Dict[str, torch.Tensor],
    clip_epsilon: float = 0.2,
    entropy_coef: float = 0.01,
    num_epochs: int = 4,
    mini_batch_size: int = 32,
) -> Dict[str, float]:
    """Perform PPO update on collected batch."""
    if not batch:
        return {"policy_loss": 0, "entropy": 0, "kl_div": 0}

    per_instr = batch["per_instr"]
    global_feats = batch["global"]
    actions = batch["actions"]
    old_log_probs = batch["old_log_probs"]
    rewards = batch["rewards"]

    # Normalize rewards
    if len(rewards) > 1:
        advantages = (rewards - rewards.mean()) / (rewards.std() + 1e-8)
    else:
        advantages = rewards

    metrics = {"policy_loss": [], "entropy": [], "kl_div": []}

    for epoch in range(num_epochs):
        # Shuffle indices
        indices = torch.randperm(len(actions))

        for start in range(0, len(actions), mini_batch_size):
            end = min(start + mini_batch_size, len(actions))
            batch_indices = indices[start:end]

            # Get mini-batch
            mb_per_instr = per_instr[batch_indices]
            mb_global = global_feats[batch_indices]
            mb_actions = actions[batch_indices]
            mb_old_log_probs = old_log_probs[batch_indices]
            mb_advantages = advantages[batch_indices]

            # Forward pass
            logits = policy(mb_per_instr, mb_global)
            log_probs = F.log_softmax(logits, dim=-1)
            probs = F.softmax(logits, dim=-1)

            # Current log probs of taken actions
            action_log_probs = log_probs.gather(1, mb_actions.unsqueeze(1)).squeeze(1)

            # Probability ratio
            ratio = torch.exp(action_log_probs - mb_old_log_probs)

            # Clipped objective
            surr1 = ratio * mb_advantages
            surr2 = torch.clamp(ratio, 1 - clip_epsilon, 1 + clip_epsilon) * mb_advantages
            policy_loss = -torch.min(surr1, surr2).mean()

            # Entropy bonus
            entropy = -(probs * log_probs).sum(dim=-1).mean()

            # KL divergence for monitoring
            with torch.no_grad():
                kl_div = (torch.exp(mb_old_log_probs) * (mb_old_log_probs - action_log_probs)).mean()

            # Total loss
            loss = policy_loss - entropy_coef * entropy

            # Backward pass
            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(policy.parameters(), max_norm=0.5)
            optimizer.step()

            metrics["policy_loss"].append(policy_loss.item())
            metrics["entropy"].append(entropy.item())
            metrics["kl_div"].append(kl_div.item())

    return {k: np.mean(v) for k, v in metrics.items()}


# ==============================================================================
# Evaluation
# ==============================================================================


def evaluate_policy(
    policy: nn.Module,
    kernels: List[Path],
    config: TrainingConfig,
    server: InferenceServer,
) -> Dict[str, float]:
    """Evaluate policy on kernel set."""
    logger = logging.getLogger("eval")
    logger.info("Starting evaluation...")

    # Switch to deterministic mode
    original_exploration = server.exploration
    server.set_exploration(False)

    stall_ratios = []
    server_address = f"{config.host}:{config.port}"

    for kernel in kernels[:min(10, len(kernels))]:  # Evaluate on subset
        success, stall_ratio, _ = run_llc_with_server(
            kernel,
            config.llc_path,
            server_address,
            config.output_dir / "eval",
            timeout=120,
        )

        if success and stall_ratio is not None:
            stall_ratios.append(stall_ratio)
            # Send reward to clear trajectory
            send_reward_to_server(server_address, 1.0 - stall_ratio, kernel.stem)

    # Restore exploration mode
    server.set_exploration(original_exploration, server.temperature)

    if stall_ratios:
        return {
            "mean_stall_ratio": np.mean(stall_ratios),
            "std_stall_ratio": np.std(stall_ratios),
            "min_stall_ratio": np.min(stall_ratios),
            "max_stall_ratio": np.max(stall_ratios),
            "num_evaluated": len(stall_ratios),
        }
    else:
        return {"error": "No successful evaluations"}


# ==============================================================================
# Main Training Loop
# ==============================================================================


def train_online(config: TrainingConfig):
    """Main online RL training loop."""
    logger = logging.getLogger("train")
    logger.info("Starting online RL training")
    logger.info(f"Config: {config}")

    # Setup directories
    config.output_dir.mkdir(parents=True, exist_ok=True)
    (config.output_dir / "checkpoints").mkdir(exist_ok=True)
    (config.output_dir / "eval").mkdir(exist_ok=True)
    (config.output_dir / "asm").mkdir(exist_ok=True)

    # Find kernels
    kernels = find_kernel_files(config.kernels_dir)
    if not kernels:
        logger.error(f"No kernel files found in {config.kernels_dir}")
        return

    logger.info(f"Found {len(kernels)} kernel files")

    # Load policy
    logger.info(f"Loading warmstart model from {config.warmstart_model}")
    policy = SchedulingPolicy()
    if config.warmstart_model.exists():
        state_dict = torch.load(config.warmstart_model, map_location=config.device)
        policy.load_state_dict(state_dict)
        logger.info("Warmstart model loaded")
    else:
        logger.warning("Warmstart model not found, training from scratch")

    policy = policy.to(config.device)
    optimizer = optim.Adam(policy.parameters(), lr=config.learning_rate)

    # Start inference server
    server = InferenceServer(
        policy=policy,
        host=config.host,
        port=config.port,
        exploration=True,
        temperature=config.initial_temperature,
        device=config.device,
    )
    server.start()
    time.sleep(1)  # Wait for server to start

    server_address = f"{config.host}:{config.port}"
    logger.info(f"Inference server started at {server_address}")

    # Training history
    history = []
    temperature = config.initial_temperature

    try:
        for iteration in range(1, config.iterations + 1):
            logger.info(f"\n=== Iteration {iteration}/{config.iterations} ===")
            logger.info(f"Temperature: {temperature:.3f}")

            # Update exploration temperature
            server.set_exploration(True, temperature)

            # Collect trajectories
            iteration_trajectories = []
            iteration_rewards = []

            for kernel in kernels:
                for sample in range(config.samples_per_kernel):
                    success, stall_ratio, _ = run_llc_with_server(
                        kernel,
                        config.llc_path,
                        server_address,
                        config.output_dir / "asm",
                    )

                    if success and stall_ratio is not None:
                        reward = 1.0 - stall_ratio  # Higher is better
                        send_reward_to_server(server_address, reward, kernel.stem)
                        iteration_rewards.append(reward)
                    else:
                        # Reset without reward on failure
                        import socket
                        try:
                            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                                sock.connect((config.host, config.port))
                                msg = {"type": "reset", "kernel": kernel.stem}
                                sock.sendall((json.dumps(msg) + "\n").encode("utf-8"))
                                sock.recv(1024)
                        except:
                            pass

            # Get collected trajectories
            trajectories = server.get_trajectories()
            logger.info(f"Collected {len(trajectories)} trajectories, "
                       f"{sum(len(t.steps) for t in trajectories)} total steps")

            if iteration_rewards:
                logger.info(f"Rewards: mean={np.mean(iteration_rewards):.4f}, "
                           f"std={np.std(iteration_rewards):.4f}")

            # PPO update
            if trajectories:
                batch = trajectories_to_batch(trajectories, config.gamma, config.device)
                if batch:
                    policy.train()
                    metrics = ppo_update(
                        policy, optimizer, batch,
                        clip_epsilon=config.clip_epsilon,
                        entropy_coef=config.entropy_coef,
                        num_epochs=config.ppo_epochs,
                        mini_batch_size=config.batch_size,
                    )
                    policy.eval()

                    logger.info(f"PPO update: loss={metrics['policy_loss']:.4f}, "
                               f"entropy={metrics['entropy']:.4f}, "
                               f"kl={metrics['kl_div']:.4f}")

                    # Update server's policy
                    server.update_policy(policy)
                else:
                    metrics = {}
            else:
                metrics = {}

            # Record history
            history_entry = {
                "iteration": iteration,
                "temperature": temperature,
                "num_trajectories": len(trajectories),
                "mean_reward": np.mean(iteration_rewards) if iteration_rewards else 0,
                **metrics,
            }
            history.append(history_entry)

            # Evaluation
            if iteration % config.eval_interval == 0:
                eval_metrics = evaluate_policy(policy, kernels, config, server)
                logger.info(f"Evaluation: {eval_metrics}")
                history[-1]["eval"] = eval_metrics

            # Checkpoint
            if iteration % config.checkpoint_interval == 0:
                checkpoint_path = config.output_dir / "checkpoints" / f"iter_{iteration:04d}.pt"
                torch.save(policy.state_dict(), checkpoint_path)
                logger.info(f"Checkpoint saved to {checkpoint_path}")

            # Decay temperature
            temperature = max(config.final_temperature, temperature * config.temperature_decay)

    except KeyboardInterrupt:
        logger.info("Training interrupted by user")
    finally:
        server.stop()

    # Save final model
    final_path = config.output_dir / "final_model.pt"
    torch.save(policy.state_dict(), final_path)
    logger.info(f"Final model saved to {final_path}")

    # Export to ONNX
    policy.eval()
    dummy_per_instr = torch.randn(1, MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES), device=config.device)
    dummy_global = torch.randn(1, len(GLOBAL_FEATURES), device=config.device)
    onnx_path = config.output_dir / "final_model.onnx"
    torch.onnx.export(
        policy, (dummy_per_instr, dummy_global),
        onnx_path,
        input_names=['per_instr', 'global'],
        output_names=['logits'],
    )
    logger.info(f"ONNX model saved to {onnx_path}")

    # Save history
    history_path = config.output_dir / "history.json"
    with open(history_path, 'w') as f:
        json.dump(history, f, indent=2)
    logger.info(f"Training history saved to {history_path}")

    # Print summary
    print("\n" + "=" * 60)
    print("Training Summary")
    print("=" * 60)
    print(f"Total iterations: {len(history)}")
    if history:
        print(f"Final mean reward: {history[-1].get('mean_reward', 0):.4f}")
        if 'eval' in history[-1]:
            print(f"Final eval stall ratio: {history[-1]['eval'].get('mean_stall_ratio', 'N/A')}")
    print(f"Model saved to: {final_path}")


# ==============================================================================
# Main
# ==============================================================================


def main():
    parser = argparse.ArgumentParser(description="Online RL training for MLGO scheduling")

    # Required paths
    parser.add_argument("--warmstart", "-w", type=Path, required=True,
                        help="Path to warmstart model (.pt file)")
    parser.add_argument("--kernels-dir", "-k", type=Path, required=True,
                        help="Directory containing kernel files (.ll or .mir)")
    parser.add_argument("--output", "-o", type=Path, required=True,
                        help="Output directory for trained models")
    parser.add_argument("--llc", type=Path,
                        default=Path("/home/jeff/source/llvm_emu_bak/llvm-project/build/bin/llc"),
                        help="Path to llc binary")

    # Server settings
    parser.add_argument("--host", default="localhost",
                        help="Host for inference server (default: localhost)")
    parser.add_argument("--port", "-p", type=int, default=50051,
                        help="Port for inference server (default: 50051)")

    # Training settings
    parser.add_argument("--iterations", type=int, default=50,
                        help="Number of training iterations (default: 50)")
    parser.add_argument("--samples-per-kernel", type=int, default=3,
                        help="Number of samples per kernel per iteration (default: 3)")
    parser.add_argument("--ppo-epochs", type=int, default=4,
                        help="PPO update epochs (default: 4)")
    parser.add_argument("--batch-size", type=int, default=32,
                        help="Mini-batch size for PPO (default: 32)")
    parser.add_argument("--learning-rate", type=float, default=3e-4,
                        help="Learning rate (default: 3e-4)")
    parser.add_argument("--clip-epsilon", type=float, default=0.2,
                        help="PPO clip epsilon (default: 0.2)")
    parser.add_argument("--gamma", type=float, default=0.99,
                        help="Discount factor (default: 0.99)")

    # Exploration settings
    parser.add_argument("--initial-temperature", type=float, default=1.0,
                        help="Initial temperature for exploration (default: 1.0)")
    parser.add_argument("--final-temperature", type=float, default=0.5,
                        help="Final temperature (default: 0.5)")

    # Evaluation/checkpointing
    parser.add_argument("--eval-interval", type=int, default=5,
                        help="Evaluate every N iterations (default: 5)")
    parser.add_argument("--checkpoint-interval", type=int, default=5,
                        help="Save checkpoint every N iterations (default: 5)")

    # Device
    parser.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu",
                        help="Device to run on (default: cuda if available)")

    # Logging
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose logging")

    args = parser.parse_args()

    # Setup logging
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    # Create config
    config = TrainingConfig(
        warmstart_model=args.warmstart,
        kernels_dir=args.kernels_dir,
        output_dir=args.output,
        llc_path=args.llc,
        host=args.host,
        port=args.port,
        iterations=args.iterations,
        samples_per_kernel=args.samples_per_kernel,
        ppo_epochs=args.ppo_epochs,
        batch_size=args.batch_size,
        learning_rate=args.learning_rate,
        clip_epsilon=args.clip_epsilon,
        gamma=args.gamma,
        initial_temperature=args.initial_temperature,
        final_temperature=args.final_temperature,
        eval_interval=args.eval_interval,
        checkpoint_interval=args.checkpoint_interval,
        device=args.device,
    )

    train_online(config)
    return 0


if __name__ == "__main__":
    sys.exit(main())
