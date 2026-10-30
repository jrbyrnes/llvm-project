#!/usr/bin/env python3
"""
PPO Reinforcement Learning Training for MLGO Instruction Scheduling using PyTorch.

This script fine-tunes a warmstart model using Proximal Policy Optimization (PPO).
Rewards are loaded from rewards.json (extracted from Static Simulator output) and
assigned to all observations from the same kernel using discounted credit assignment.

The "total reward" approach: All scheduling decisions in a kernel share the final
reward (stall ratio), with optional discounting based on position in the sequence.

Usage:
    # Basic PPO training from warmstart
    python3 train_ppo_pytorch.py --warmstart models/warmstart/model.pt \
        --data-dir logs/heuristic/ --output models/ppo/

    # With custom hyperparameters
    python3 train_ppo_pytorch.py --warmstart models/warmstart/model.pt \
        --data-dir logs/heuristic/ --output models/ppo/ \
        --iterations 100 --gamma 0.99 --clip-epsilon 0.2
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Any, Optional

import numpy as np

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


# ==============================================================================
# Feature Specification (must match MLCoExecSchedStrategy.cpp)
# ==============================================================================

MAX_NUM_CANDIDATES = 256
NUM_OUTPUTS = MAX_NUM_CANDIDATES * 2

PER_INSTR_FEATURES = [
    "mask", "is_top", "is_bot", "pos", "excess", "current_max", "critical_max",
    "su_latency", "su_height", "su_depth", "su_succs_left", "su_preds_left",
    "su_succs", "su_preds",
]

GLOBAL_FEATURES = [
    "sgpr_critical_limit", "vgpr_critical_limit", "sgpr_excess_limit", "vgpr_excess_limit",
]

DECISION_KEY = "index_to_sched"


# ==============================================================================
# Log Parsing with Episode-aware Reward Assignment
# ==============================================================================


def parse_llvm_log_with_episodes(log_path: Path, kernel_reward: float, gamma: float = 0.99) -> List[Dict]:
    """
    Parse LLVM log and assign discounted rewards.

    For a kernel with N scheduling decisions and final reward R:
    - Decision i gets reward: R * gamma^(N-1-i)
    - This gives more credit to later decisions (closer to the outcome)

    With gamma=1.0, all decisions get the same reward (total reward approach).
    """
    observations = []

    with open(log_path, 'rb') as f:
        header_line = f.readline()
        try:
            header = json.loads(header_line.decode('utf-8'))
        except json.JSONDecodeError:
            return []

        feature_specs = {}
        for feat in header.get('features', []):
            feature_specs[feat['name']] = {'shape': feat['shape'], 'type': feat['type']}

        while True:
            line = f.readline()
            if not line:
                break

            try:
                line_json = json.loads(line.decode('utf-8').strip())
            except json.JSONDecodeError:
                continue

            if 'observation' in line_json:
                obs_data = {}
                for name, spec in feature_specs.items():
                    size = 1
                    for dim in spec['shape']:
                        size *= dim

                    if 'int64' in spec['type']:
                        raw = f.read(size * 8)
                        if len(raw) < size * 8:
                            break
                        values = struct.unpack(f'<{size}q', raw)
                        obs_data[name] = np.array(values, dtype=np.int64).reshape(spec['shape'])
                    elif 'float' in spec['type']:
                        raw = f.read(size * 4)
                        if len(raw) < size * 4:
                            break
                        values = struct.unpack(f'<{size}f', raw)
                        obs_data[name] = np.array(values, dtype=np.float32).reshape(spec['shape'])

                if len(obs_data) == len(feature_specs):
                    observations.append(obs_data)

            elif 'outcome' in line_json:
                f.read(4)  # Skip logged reward

    # Assign discounted rewards
    N = len(observations)
    for i, obs in enumerate(observations):
        # Discounted reward: later decisions get higher weight
        obs['reward'] = kernel_reward * (gamma ** (N - 1 - i))
        obs['position'] = i
        obs['episode_length'] = N

    return observations


def load_ppo_training_data(data_dir: Path, gamma: float = 0.99) -> Tuple[Dict[str, np.ndarray], np.ndarray, np.ndarray]:
    """Load training data with episode-aware reward assignment."""
    all_observations = []

    # Load rewards
    rewards_path = data_dir / "rewards.json"
    kernel_rewards = {}
    if rewards_path.exists():
        with open(rewards_path) as f:
            rewards_data = json.load(f)
            for name, data in rewards_data.items():
                if data.get('reward') is not None:
                    kernel_rewards[name] = data['reward']
        print(f"Loaded rewards for {len(kernel_rewards)} kernels")
    else:
        raise ValueError("rewards.json not found - run collect_training_data.py first")

    # Find log files
    log_files = list(data_dir.glob("*.tfrecord"))
    log_files = [f for f in log_files if f.stem in kernel_rewards]

    if not log_files:
        raise ValueError(f"No log files with rewards found in {data_dir}")

    print(f"Found {len(log_files)} log file(s) with rewards")

    for log_file in log_files:
        kernel_name = log_file.stem
        reward = kernel_rewards[kernel_name]

        print(f"  Loading: {log_file.name} (reward={reward:.3f}, gamma={gamma})...")
        observations = parse_llvm_log_with_episodes(log_file, reward, gamma)
        print(f"    {len(observations)} observations")
        all_observations.extend(observations)

    print(f"Total observations: {len(all_observations)}")

    # Convert to arrays
    features = {feat: [] for feat in PER_INSTR_FEATURES + GLOBAL_FEATURES}
    decisions = []
    rewards = []

    for obs in all_observations:
        if DECISION_KEY not in obs:
            continue

        for feat in PER_INSTR_FEATURES:
            features[feat].append(obs.get(feat, np.zeros((1, MAX_NUM_CANDIDATES))).flatten())
        for feat in GLOBAL_FEATURES:
            features[feat].append(obs.get(feat, np.zeros(1)).flatten())

        decisions.append(obs[DECISION_KEY].flatten()[0])
        rewards.append(obs['reward'])

    feature_arrays = {k: np.array(v, dtype=np.float32) for k, v in features.items()}
    decision_array = np.array(decisions, dtype=np.int64)
    reward_array = np.array(rewards, dtype=np.float32)

    print(f"Loaded {len(decision_array)} training examples")
    print(f"Reward stats: min={reward_array.min():.3f}, max={reward_array.max():.3f}, mean={reward_array.mean():.3f}")

    return feature_arrays, decision_array, reward_array


# ==============================================================================
# Model (same as warmstart)
# ==============================================================================


class SchedulingPolicy(nn.Module):
    def __init__(self, embedding_dim: int = 64, num_attention_heads: int = 4,
                 ff_dim: int = 128, dropout_rate: float = 0.1):
        super().__init__()

        self.per_instr_embed = nn.Sequential(
            nn.Linear(len(PER_INSTR_FEATURES), embedding_dim),
            nn.ReLU(),
            nn.LayerNorm(embedding_dim),
        )

        self.attention = nn.MultiheadAttention(
            embed_dim=embedding_dim, num_heads=num_attention_heads,
            dropout=dropout_rate, batch_first=True,
        )
        self.attention_norm = nn.LayerNorm(embedding_dim)

        self.ff = nn.Sequential(
            nn.Linear(embedding_dim, ff_dim), nn.ReLU(),
            nn.Linear(ff_dim, embedding_dim), nn.Dropout(dropout_rate),
        )
        self.ff_norm = nn.LayerNorm(embedding_dim)

        self.output_head = nn.Sequential(
            nn.Linear(embedding_dim * MAX_NUM_CANDIDATES + len(GLOBAL_FEATURES), 256),
            nn.ReLU(), nn.Dropout(dropout_rate),
            nn.Linear(256, 128), nn.ReLU(),
            nn.Linear(128, NUM_OUTPUTS),
        )

    def forward(self, per_instr: torch.Tensor, global_feats: torch.Tensor) -> torch.Tensor:
        x = self.per_instr_embed(per_instr)
        attn_out, _ = self.attention(x, x, x)
        x = self.attention_norm(x + attn_out)
        ff_out = self.ff(x)
        x = self.ff_norm(x + ff_out)
        x = x.flatten(start_dim=1)
        x = torch.cat([x, global_feats], dim=-1)
        return self.output_head(x)


# ==============================================================================
# PPO Dataset
# ==============================================================================


class PPODataset(Dataset):
    def __init__(self, features: Dict[str, np.ndarray], decisions: np.ndarray, rewards: np.ndarray):
        self.features = {k: torch.from_numpy(v) for k, v in features.items()}
        self.decisions = torch.from_numpy(decisions)
        self.rewards = torch.from_numpy(rewards)

        valid_mask = (self.decisions >= 0) & (self.decisions < NUM_OUTPUTS)
        self.valid_indices = torch.where(valid_mask)[0]

    def __len__(self):
        return len(self.valid_indices)

    def __getitem__(self, idx):
        real_idx = self.valid_indices[idx]
        per_instr = torch.stack([self.features[f][real_idx] for f in PER_INSTR_FEATURES], dim=-1)
        global_feats = torch.cat([self.features[f][real_idx] for f in GLOBAL_FEATURES])

        return {
            'per_instr': per_instr,
            'global': global_feats,
            'decision': self.decisions[real_idx],
            'reward': self.rewards[real_idx],
        }


# ==============================================================================
# PPO Training
# ==============================================================================


def compute_ppo_loss(
    policy: nn.Module,
    batch: Dict[str, torch.Tensor],
    old_log_probs: torch.Tensor,
    advantages: torch.Tensor,
    clip_epsilon: float = 0.2,
    entropy_coef: float = 0.01,
) -> Tuple[torch.Tensor, Dict[str, float]]:
    """Compute PPO clipped objective loss."""
    logits = policy(batch['per_instr'], batch['global'])
    log_probs = F.log_softmax(logits, dim=-1)

    # Get log prob of taken action
    action_log_probs = log_probs.gather(1, batch['decision'].unsqueeze(1)).squeeze(1)

    # Probability ratio
    ratio = torch.exp(action_log_probs - old_log_probs)

    # Clipped objective
    surr1 = ratio * advantages
    surr2 = torch.clamp(ratio, 1 - clip_epsilon, 1 + clip_epsilon) * advantages
    policy_loss = -torch.min(surr1, surr2).mean()

    # Entropy bonus (for exploration)
    probs = F.softmax(logits, dim=-1)
    entropy = -(probs * log_probs).sum(dim=-1).mean()

    total_loss = policy_loss - entropy_coef * entropy

    metrics = {
        'policy_loss': policy_loss.item(),
        'entropy': entropy.item(),
        'ratio_mean': ratio.mean().item(),
    }

    return total_loss, metrics


def train_ppo(
    policy: nn.Module,
    dataset: PPODataset,
    output_dir: Path,
    iterations: int = 100,
    batch_size: int = 64,
    ppo_epochs: int = 4,
    learning_rate: float = 3e-4,
    clip_epsilon: float = 0.2,
    entropy_coef: float = 0.01,
    gamma: float = 0.99,
    device: str = "cuda" if torch.cuda.is_available() else "cpu",
    checkpoint_interval: int = 10,
):
    """Train policy using PPO."""
    output_dir.mkdir(parents=True, exist_ok=True)

    policy = policy.to(device)
    optimizer = optim.Adam(policy.parameters(), lr=learning_rate)

    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    history = []

    for iteration in range(1, iterations + 1):
        # Collect data with current policy
        all_log_probs = []
        all_rewards = []

        policy.eval()
        with torch.no_grad():
            for batch in dataloader:
                per_instr = batch['per_instr'].to(device)
                global_feats = batch['global'].to(device)
                decisions = batch['decision'].to(device)
                rewards = batch['reward'].to(device)

                logits = policy(per_instr, global_feats)
                log_probs = F.log_softmax(logits, dim=-1)
                action_log_probs = log_probs.gather(1, decisions.unsqueeze(1)).squeeze(1)

                all_log_probs.append(action_log_probs)
                all_rewards.append(rewards)

        old_log_probs = torch.cat(all_log_probs)
        rewards = torch.cat(all_rewards)

        # Compute advantages (rewards are already discounted per-episode)
        # Normalize advantages for stability
        advantages = (rewards - rewards.mean()) / (rewards.std() + 1e-8)

        # PPO update epochs
        policy.train()
        epoch_metrics = []

        idx = 0
        for epoch in range(ppo_epochs):
            for batch in dataloader:
                batch_size_actual = len(batch['decision'])
                batch_old_log_probs = old_log_probs[idx:idx + batch_size_actual].to(device)
                batch_advantages = advantages[idx:idx + batch_size_actual].to(device)

                batch = {k: v.to(device) for k, v in batch.items()}

                optimizer.zero_grad()
                loss, metrics = compute_ppo_loss(
                    policy, batch, batch_old_log_probs, batch_advantages,
                    clip_epsilon=clip_epsilon, entropy_coef=entropy_coef,
                )
                loss.backward()
                optimizer.step()

                epoch_metrics.append(metrics)
                idx = (idx + batch_size_actual) % len(old_log_probs)

        # Aggregate metrics
        avg_metrics = {
            k: np.mean([m[k] for m in epoch_metrics])
            for k in epoch_metrics[0].keys()
        }
        avg_metrics['mean_reward'] = rewards.mean().item()
        avg_metrics['iteration'] = iteration

        history.append(avg_metrics)

        print(f"Iteration {iteration}/{iterations}: "
              f"policy_loss={avg_metrics['policy_loss']:.4f}, "
              f"entropy={avg_metrics['entropy']:.4f}, "
              f"mean_reward={avg_metrics['mean_reward']:.4f}")

        # Checkpoint
        if iteration % checkpoint_interval == 0:
            checkpoint_dir = output_dir / f"iter_{iteration:04d}"
            checkpoint_dir.mkdir(exist_ok=True)
            torch.save(policy.state_dict(), checkpoint_dir / 'policy.pt')

            # Export ONNX
            policy.eval()
            dummy_per_instr = torch.randn(1, MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES)).to(device)
            dummy_global = torch.randn(1, len(GLOBAL_FEATURES)).to(device)
            torch.onnx.export(
                policy, (dummy_per_instr, dummy_global),
                checkpoint_dir / 'model.onnx',
                input_names=['per_instr', 'global'],
                output_names=['logits'],
            )
            print(f"  Checkpoint saved to {checkpoint_dir}")

    # Save final model
    final_dir = output_dir / "final"
    final_dir.mkdir(exist_ok=True)
    torch.save(policy.state_dict(), final_dir / 'policy.pt')

    policy.eval()
    dummy_per_instr = torch.randn(1, MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES)).to(device)
    dummy_global = torch.randn(1, len(GLOBAL_FEATURES)).to(device)
    torch.onnx.export(
        policy, (dummy_per_instr, dummy_global),
        final_dir / 'model.onnx',
        input_names=['per_instr', 'global'],
        output_names=['logits'],
    )

    # Save history
    with open(output_dir / 'history.json', 'w') as f:
        json.dump(history, f, indent=2)

    print(f"\nTraining complete! Final model saved to {final_dir}")

    return history


# ==============================================================================
# Main
# ==============================================================================


def main():
    parser = argparse.ArgumentParser(description="PPO training for MLGO scheduling")
    parser.add_argument("--warmstart", "-w", type=Path, required=True,
                        help="Path to warmstart model (.pt file)")
    parser.add_argument("--data-dir", "-d", type=Path, required=True,
                        help="Directory containing training logs and rewards.json")
    parser.add_argument("--output", "-o", type=Path, required=True,
                        help="Output directory for trained models")
    parser.add_argument("--iterations", type=int, default=100,
                        help="Number of PPO iterations (default: 100)")
    parser.add_argument("--batch-size", type=int, default=64,
                        help="Batch size (default: 64)")
    parser.add_argument("--ppo-epochs", type=int, default=4,
                        help="PPO update epochs per iteration (default: 4)")
    parser.add_argument("--learning-rate", type=float, default=3e-4,
                        help="Learning rate (default: 3e-4)")
    parser.add_argument("--clip-epsilon", type=float, default=0.2,
                        help="PPO clip epsilon (default: 0.2)")
    parser.add_argument("--gamma", type=float, default=0.99,
                        help="Discount factor for credit assignment (default: 0.99)")
    parser.add_argument("--checkpoint-interval", type=int, default=10,
                        help="Save checkpoint every N iterations (default: 10)")
    args = parser.parse_args()

    # Load data with proper reward assignment
    print("Loading training data with episode-aware rewards...")
    features, decisions, rewards = load_ppo_training_data(args.data_dir, gamma=args.gamma)

    dataset = PPODataset(features, decisions, rewards)
    print(f"Dataset: {len(dataset)} examples")

    # Create and load warmstart model
    print(f"\nLoading warmstart model from {args.warmstart}...")
    policy = SchedulingPolicy()

    if args.warmstart.exists():
        state_dict = torch.load(args.warmstart, map_location='cpu')
        policy.load_state_dict(state_dict)
        print("Warmstart model loaded successfully")
    else:
        print("WARNING: Warmstart model not found, training from scratch")

    print(f"Model parameters: {sum(p.numel() for p in policy.parameters()):,}")

    # Train
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"\nStarting PPO training on {device}...")

    history = train_ppo(
        policy, dataset, args.output,
        iterations=args.iterations,
        batch_size=args.batch_size,
        ppo_epochs=args.ppo_epochs,
        learning_rate=args.learning_rate,
        clip_epsilon=args.clip_epsilon,
        gamma=args.gamma,
        device=device,
        checkpoint_interval=args.checkpoint_interval,
    )

    # Summary
    print("\n" + "="*60)
    print("Training Summary:")
    print(f"  Final policy loss: {history[-1]['policy_loss']:.4f}")
    print(f"  Final entropy: {history[-1]['entropy']:.4f}")
    print(f"  Final mean reward: {history[-1]['mean_reward']:.4f}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
