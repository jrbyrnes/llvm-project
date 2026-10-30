#!/usr/bin/env python3
"""
Inference Server for MLGO Online Reinforcement Learning.

This TCP server serves as the bridge between LLVM (which performs instruction scheduling)
and Python (which runs the policy model and collects trajectories for training).

Protocol:
    Client -> Server: {"type": "features", "data": {...}}
    Server -> Client: {"action": 42, "log_prob": -2.3}

    Client -> Server: {"type": "reward", "value": 0.35}
    Server -> Client: {"status": "ok"}

    Client -> Server: {"type": "reset"}  # New episode
    Server -> Client: {"status": "ok"}

Usage:
    python3 inference_server.py --model models/warmstart/model.pt --port 50051

    # With exploration (for training)
    python3 inference_server.py --model models/warmstart/model.pt --port 50051 --exploration

    # Deterministic (for evaluation)
    python3 inference_server.py --model models/warmstart/model.pt --port 50051 --deterministic
"""

from __future__ import annotations

import argparse
import json
import logging
import socket
import struct
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

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


# ==============================================================================
# Model (same architecture as training scripts)
# ==============================================================================


class SchedulingPolicy(nn.Module):
    """Neural network for instruction scheduling policy."""

    def __init__(
        self,
        num_per_instr_features: int = len(PER_INSTR_FEATURES),
        num_global_features: int = len(GLOBAL_FEATURES),
        embedding_dim: int = 64,
        num_attention_heads: int = 4,
        ff_dim: int = 128,
        dropout_rate: float = 0.1,
    ):
        super().__init__()

        self.per_instr_embed = nn.Sequential(
            nn.Linear(num_per_instr_features, embedding_dim),
            nn.ReLU(),
            nn.LayerNorm(embedding_dim),
        )

        self.attention = nn.MultiheadAttention(
            embed_dim=embedding_dim,
            num_heads=num_attention_heads,
            dropout=dropout_rate,
            batch_first=True,
        )
        self.attention_norm = nn.LayerNorm(embedding_dim)

        self.ff = nn.Sequential(
            nn.Linear(embedding_dim, ff_dim),
            nn.ReLU(),
            nn.Linear(ff_dim, embedding_dim),
            nn.Dropout(dropout_rate),
        )
        self.ff_norm = nn.LayerNorm(embedding_dim)

        self.output_head = nn.Sequential(
            nn.Linear(embedding_dim * MAX_NUM_CANDIDATES + num_global_features, 256),
            nn.ReLU(),
            nn.Dropout(dropout_rate),
            nn.Linear(256, 128),
            nn.ReLU(),
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
# Trajectory Storage
# ==============================================================================


@dataclass
class Step:
    """A single step in a trajectory."""
    state: Dict[str, np.ndarray]
    action: int
    log_prob: float


@dataclass
class Trajectory:
    """A complete episode trajectory."""
    steps: List[Step] = field(default_factory=list)
    reward: float = 0.0
    kernel_name: str = ""


# ==============================================================================
# Inference Server
# ==============================================================================


class InferenceServer:
    """TCP server for LLVM-Python inference coordination."""

    def __init__(
        self,
        policy: nn.Module,
        host: str = "localhost",
        port: int = 50051,
        exploration: bool = True,
        temperature: float = 1.0,
        device: str = "cpu",
    ):
        self.policy = policy.to(device)
        self.policy.eval()
        self.host = host
        self.port = port
        self.exploration = exploration
        self.temperature = temperature
        self.device = device

        # Current episode state
        # pending_trajectory: trajectory being built, awaiting reward
        # current_trajectory: new trajectory for next episode
        self.pending_trajectory: Optional[Trajectory] = None
        self.current_trajectory = Trajectory()
        self.completed_trajectories: List[Trajectory] = []
        self.trajectory_lock = threading.Lock()

        # Server state
        self.server_socket: Optional[socket.socket] = None
        self.running = False
        self.server_thread: Optional[threading.Thread] = None

        # Logging
        self.logger = logging.getLogger("InferenceServer")

    def _features_to_tensors(self, data: Dict[str, Any]) -> Tuple[torch.Tensor, torch.Tensor]:
        """Convert feature dict to model input tensors."""
        per_instr_data = data.get("per_instr", {})
        global_data = data.get("global", {})

        # Build per-instruction features [1, MAX_NUM_CANDIDATES, num_features]
        per_instr_list = []
        for feat in PER_INSTR_FEATURES:
            values = per_instr_data.get(feat, [0] * MAX_NUM_CANDIDATES)
            if len(values) < MAX_NUM_CANDIDATES:
                values = values + [0] * (MAX_NUM_CANDIDATES - len(values))
            per_instr_list.append(values[:MAX_NUM_CANDIDATES])

        per_instr = np.array(per_instr_list, dtype=np.float32).T  # [MAX_NUM_CANDIDATES, num_features]
        per_instr = torch.from_numpy(per_instr).unsqueeze(0).to(self.device)

        # Build global features [1, num_global_features]
        global_list = []
        for feat in GLOBAL_FEATURES:
            value = global_data.get(feat, 0)
            if isinstance(value, list):
                value = value[0] if value else 0
            global_list.append(float(value))

        global_feats = torch.tensor([global_list], dtype=torch.float32, device=self.device)

        return per_instr, global_feats

    def _infer(self, data: Dict[str, Any]) -> Tuple[int, float]:
        """Run inference and return action and log probability."""
        per_instr, global_feats = self._features_to_tensors(data)

        with torch.no_grad():
            logits = self.policy(per_instr, global_feats)

            # Apply temperature
            logits = logits / self.temperature

            # Get valid action mask from features
            mask_data = data.get("per_instr", {}).get("mask", [])
            valid_mask = torch.ones(NUM_OUTPUTS, dtype=torch.bool, device=self.device)
            if mask_data:
                # Create mask for valid actions (both top and bot queues)
                valid_mask = torch.zeros(NUM_OUTPUTS, dtype=torch.bool, device=self.device)
                for i, m in enumerate(mask_data[:MAX_NUM_CANDIDATES]):
                    if m:
                        valid_mask[2 * i] = True      # Bot queue
                        valid_mask[2 * i + 1] = True  # Top queue

                # If no valid actions, allow all (fallback to avoid NaN)
                if not valid_mask.any():
                    self.logger.warning("No valid actions in mask, allowing all")
                    valid_mask = torch.ones(NUM_OUTPUTS, dtype=torch.bool, device=self.device)

                # Mask invalid actions with large negative value
                logits = logits.masked_fill(~valid_mask.unsqueeze(0), float('-inf'))

            # Compute probabilities with numerical stability
            logits_max = logits.max(dim=-1, keepdim=True)[0]
            logits_stable = logits - logits_max
            probs = F.softmax(logits_stable, dim=-1)
            log_probs = F.log_softmax(logits_stable, dim=-1)

            # Safety check for NaN (shouldn't happen with above fixes)
            if torch.isnan(probs).any():
                self.logger.warning("NaN in probs, using uniform distribution")
                probs = valid_mask.float().unsqueeze(0)
                probs = probs / probs.sum()
                log_probs = torch.log(probs + 1e-10)

            if self.exploration:
                # Sample from distribution
                dist = torch.distributions.Categorical(probs)
                action = dist.sample().item()
            else:
                # Greedy selection
                action = probs.argmax(dim=-1).item()

            log_prob = log_probs[0, action].item()

        return action, log_prob

    def _handle_client(self, client_socket: socket.socket, address: Tuple[str, int]):
        """Handle a single client connection."""
        self.logger.info(f"Client connected from {address}")

        buffer = b""
        while self.running:
            try:
                # Receive data
                data = client_socket.recv(4096)
                if not data:
                    break

                buffer += data

                # Process complete messages (newline-delimited JSON)
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    if not line:
                        continue

                    try:
                        msg = json.loads(line.decode("utf-8"))
                    except json.JSONDecodeError as e:
                        self.logger.error(f"JSON decode error: {e}")
                        continue

                    response = self._process_message(msg)
                    response_bytes = (json.dumps(response) + "\n").encode("utf-8")
                    client_socket.sendall(response_bytes)

            except socket.timeout:
                continue
            except Exception as e:
                self.logger.error(f"Error handling client: {e}")
                break

        client_socket.close()
        self.logger.info(f"Client disconnected: {address}")

    def _process_message(self, msg: Dict[str, Any]) -> Dict[str, Any]:
        """Process a single message and return response."""
        msg_type = msg.get("type", "")

        if msg_type == "features":
            # Run inference
            data = msg.get("data", {})
            action, log_prob = self._infer(data)

            # Store step in trajectory
            with self.trajectory_lock:
                # Convert data to numpy for storage
                state = {}
                for feat in PER_INSTR_FEATURES:
                    values = data.get("per_instr", {}).get(feat, [])
                    state[feat] = np.array(values, dtype=np.float32)
                for feat in GLOBAL_FEATURES:
                    value = data.get("global", {}).get(feat, 0)
                    state[feat] = np.array([value], dtype=np.float32)

                self.current_trajectory.steps.append(Step(
                    state=state,
                    action=action,
                    log_prob=log_prob,
                ))

            self.logger.debug(f"Inference: action={action}, log_prob={log_prob:.4f}")
            return {"action": action, "log_prob": log_prob}

        elif msg_type == "end_episode":
            # llc finished scheduling - move current trajectory to pending
            # (awaiting reward from separate connection)
            with self.trajectory_lock:
                if self.current_trajectory.steps:
                    # Discard old pending if exists (shouldn't happen in normal flow)
                    if self.pending_trajectory and self.pending_trajectory.steps:
                        self.logger.warning(f"Discarding old pending trajectory: {len(self.pending_trajectory.steps)} steps")
                    self.pending_trajectory = self.current_trajectory
                    self.current_trajectory = Trajectory()
                    self.logger.info(f"Episode ended: {len(self.pending_trajectory.steps)} steps, awaiting reward")
            return {"status": "ok"}

        elif msg_type == "reward":
            # Receive final reward for episode (comes on separate connection after llc finishes)
            reward = msg.get("value", 0.0)
            kernel_name = msg.get("kernel", "")

            with self.trajectory_lock:
                # First check pending trajectory (from finished llc run)
                traj_to_complete = self.pending_trajectory if self.pending_trajectory and self.pending_trajectory.steps else self.current_trajectory

                traj_to_complete.reward = reward
                traj_to_complete.kernel_name = kernel_name

                if traj_to_complete.steps:
                    self.completed_trajectories.append(traj_to_complete)
                    self.logger.info(
                        f"Episode complete: {len(traj_to_complete.steps)} steps, "
                        f"reward={reward:.4f}, kernel={kernel_name}"
                    )
                else:
                    self.logger.warning(f"Reward received but no trajectory steps for {kernel_name}")

                # Reset both
                self.pending_trajectory = None
                self.current_trajectory = Trajectory()

            return {"status": "ok"}

        elif msg_type == "reset":
            # Reset for new episode (without reward - e.g., abort or failed compilation)
            kernel_name = msg.get("kernel", "")
            with self.trajectory_lock:
                # Discard both pending and current
                total_discarded = 0
                if self.pending_trajectory and self.pending_trajectory.steps:
                    total_discarded += len(self.pending_trajectory.steps)
                if self.current_trajectory.steps:
                    total_discarded += len(self.current_trajectory.steps)
                if total_discarded > 0:
                    self.logger.info(f"Episode reset (no reward): {total_discarded} steps discarded")
                self.pending_trajectory = None
                self.current_trajectory = Trajectory()
                self.current_trajectory.kernel_name = kernel_name
            return {"status": "ok"}

        elif msg_type == "ping":
            return {"status": "pong"}

        elif msg_type == "stats":
            # Return current statistics
            with self.trajectory_lock:
                pending_steps = len(self.pending_trajectory.steps) if self.pending_trajectory else 0
                return {
                    "completed_episodes": len(self.completed_trajectories),
                    "current_steps": len(self.current_trajectory.steps),
                    "pending_steps": pending_steps,
                    "exploration": self.exploration,
                    "temperature": self.temperature,
                }

        else:
            self.logger.warning(f"Unknown message type: {msg_type}")
            return {"status": "error", "message": f"Unknown type: {msg_type}"}

    def start(self):
        """Start the server in a background thread."""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        self.server_socket.settimeout(1.0)

        self.running = True
        self.server_thread = threading.Thread(target=self._server_loop, daemon=True)
        self.server_thread.start()

        self.logger.info(f"Server started on {self.host}:{self.port}")

    def _server_loop(self):
        """Main server loop accepting connections."""
        while self.running:
            try:
                client_socket, address = self.server_socket.accept()
                client_socket.settimeout(5.0)
                client_thread = threading.Thread(
                    target=self._handle_client,
                    args=(client_socket, address),
                    daemon=True,
                )
                client_thread.start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    self.logger.error(f"Server error: {e}")

    def stop(self):
        """Stop the server."""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        if self.server_thread:
            self.server_thread.join(timeout=2.0)
        self.logger.info("Server stopped")

    def get_trajectories(self) -> List[Trajectory]:
        """Get and clear completed trajectories."""
        with self.trajectory_lock:
            trajectories = self.completed_trajectories
            self.completed_trajectories = []
        return trajectories

    def get_current_trajectory(self) -> Trajectory:
        """Get the current (incomplete) trajectory."""
        with self.trajectory_lock:
            return self.current_trajectory

    def update_policy(self, new_policy: nn.Module):
        """Update the policy model (thread-safe)."""
        with self.trajectory_lock:
            self.policy = new_policy.to(self.device)
            self.policy.eval()
        self.logger.info("Policy updated")

    def set_exploration(self, exploration: bool, temperature: float = 1.0):
        """Update exploration settings."""
        self.exploration = exploration
        self.temperature = temperature
        self.logger.info(f"Exploration: {exploration}, temperature: {temperature}")


# ==============================================================================
# Standalone Server Mode
# ==============================================================================


def main():
    parser = argparse.ArgumentParser(description="MLGO Inference Server")
    parser.add_argument("--model", "-m", type=Path, required=True,
                        help="Path to model checkpoint (.pt file)")
    parser.add_argument("--host", default="localhost",
                        help="Host to bind to (default: localhost)")
    parser.add_argument("--port", "-p", type=int, default=50051,
                        help="Port to listen on (default: 50051)")
    parser.add_argument("--exploration", action="store_true",
                        help="Enable exploration (sample from distribution)")
    parser.add_argument("--deterministic", action="store_true",
                        help="Deterministic mode (argmax)")
    parser.add_argument("--temperature", type=float, default=1.0,
                        help="Temperature for sampling (default: 1.0)")
    parser.add_argument("--device", default="cpu",
                        help="Device to run on (default: cpu)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose logging")
    args = parser.parse_args()

    # Setup logging
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    logger = logging.getLogger("main")

    # Load model
    logger.info(f"Loading model from {args.model}")
    policy = SchedulingPolicy()

    if args.model.exists():
        state_dict = torch.load(args.model, map_location=args.device)
        policy.load_state_dict(state_dict)
        logger.info("Model loaded successfully")
    else:
        logger.warning(f"Model not found at {args.model}, using random weights")

    # Determine exploration mode
    exploration = args.exploration or not args.deterministic

    # Create and start server
    server = InferenceServer(
        policy=policy,
        host=args.host,
        port=args.port,
        exploration=exploration,
        temperature=args.temperature,
        device=args.device,
    )

    try:
        server.start()
        logger.info(f"Server running. Press Ctrl+C to stop.")
        logger.info(f"Exploration: {exploration}, Temperature: {args.temperature}")

        # Keep running and periodically log stats
        while True:
            time.sleep(10)
            trajectories = server.get_trajectories()
            if trajectories:
                logger.info(f"Collected {len(trajectories)} trajectories")
                for t in trajectories:
                    logger.info(f"  - {t.kernel_name}: {len(t.steps)} steps, reward={t.reward:.4f}")

    except KeyboardInterrupt:
        logger.info("Shutting down...")
    finally:
        server.stop()

    return 0


if __name__ == "__main__":
    sys.exit(main())
