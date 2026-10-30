#!/usr/bin/env python3
"""
Behavioral Cloning (Warmstart) Training for MLGO Instruction Scheduling using PyTorch.

This script trains a neural network to imitate the heuristic scheduler's decisions
using supervised learning on collected LLVM MLGO logs.

Post-hoc reward assignment: Rewards are loaded from rewards.json (extracted from
Static Simulator output) and assigned to all observations from the same kernel.

Usage:
    # Basic training
    python3 train_warmstart_pytorch.py --data logs/heuristic/ --output models/warmstart/

    # With custom hyperparameters
    python3 train_warmstart_pytorch.py --data logs/heuristic/ --output models/warmstart/ \
        --epochs 100 --batch-size 64 --learning-rate 0.001
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
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split


# ==============================================================================
# Feature Specification (must match MLCoExecSchedStrategy.cpp)
# ==============================================================================

MAX_NUM_CANDIDATES = 256  # MaxNumInstCandidates = 128 * 2

# Per-instruction features (shape: [1, MAX_NUM_CANDIDATES])
PER_INSTR_FEATURES = [
    "mask",
    "is_top",
    "is_bot",
    "pos",
    "excess",
    "current_max",
    "critical_max",
    "su_latency",
    "su_height",
    "su_depth",
    "su_succs_left",
    "su_preds_left",
    "su_succs",
    "su_preds",
]

# Global features (shape: [1])
GLOBAL_FEATURES = [
    "sgpr_critical_limit",
    "vgpr_critical_limit",
    "sgpr_excess_limit",
    "vgpr_excess_limit",
]

# Decision: SchedIndex = 2 * Pos + IsTopNode
# Output space: 512 positions (256 candidates * 2 queues)
NUM_OUTPUTS = MAX_NUM_CANDIDATES * 2
DECISION_KEY = "index_to_sched"


# ==============================================================================
# LLVM MLGO Log Parsing with Post-hoc Reward Assignment
# ==============================================================================


def parse_llvm_log(log_path: Path, kernel_reward: Optional[float] = None) -> List[Dict[str, Any]]:
    """
    Parse LLVM MLGO log format and assign rewards.

    Args:
        log_path: Path to the .tfrecord log file
        kernel_reward: The reward to assign to all observations (from Static Simulator)
    """
    observations = []

    with open(log_path, 'rb') as f:
        # Read header line
        header_line = f.readline()
        try:
            header = json.loads(header_line.decode('utf-8'))
        except json.JSONDecodeError as e:
            print(f"Error parsing header in {log_path}: {e}")
            return []

        # Parse feature specs from header
        feature_specs = {}
        for feat in header.get('features', []):
            name = feat['name']
            shape = feat['shape']
            dtype = feat['type']
            feature_specs[name] = {'shape': shape, 'type': dtype}

        # Read observations
        while True:
            line = f.readline()
            if not line:
                break

            try:
                line_json = json.loads(line.decode('utf-8').strip())
            except json.JSONDecodeError:
                continue

            if 'observation' in line_json:
                # Read binary feature data
                obs_data = {}

                for name in feature_specs.keys():
                    spec = feature_specs[name]
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
                    # Assign the kernel-level reward to this observation
                    obs_data['reward'] = kernel_reward if kernel_reward is not None else 0.0
                    observations.append(obs_data)

            elif 'outcome' in line_json:
                # Skip the logged reward (it's 0.0), we use post-hoc reward
                f.read(4)

    return observations


def load_training_data(data_dir: Path) -> Tuple[Dict[str, np.ndarray], np.ndarray, np.ndarray]:
    """
    Load all training data from a directory of log files.

    Returns:
        features: Dict of feature arrays
        decisions: Array of decision indices
        rewards: Array of rewards (from post-hoc assignment)
    """
    all_observations = []

    # Load rewards from summary
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
        print("WARNING: rewards.json not found, using reward=0.0 for all observations")

    # Find all log files
    log_files = list(data_dir.glob("*.tfrecord")) + list(data_dir.glob("*.log"))
    log_files = [f for f in log_files if f.stem != "summary" and f.stem != "rewards"]

    if not log_files:
        raise ValueError(f"No log files found in {data_dir}")

    print(f"Found {len(log_files)} log file(s)")

    for log_file in log_files:
        kernel_name = log_file.stem
        reward = kernel_rewards.get(kernel_name, 0.0)

        print(f"  Loading: {log_file.name} (reward={reward:.3f})...")
        observations = parse_llvm_log(log_file, kernel_reward=reward)
        print(f"    {len(observations)} observations")
        all_observations.extend(observations)

    if not all_observations:
        raise ValueError("No observations loaded from any log file")

    print(f"Total observations: {len(all_observations)}")

    # Convert to arrays
    features = {feat: [] for feat in PER_INSTR_FEATURES + GLOBAL_FEATURES}
    decisions = []
    rewards = []

    for obs in all_observations:
        # Skip observations without a decision
        if DECISION_KEY not in obs:
            continue

        for feat in PER_INSTR_FEATURES:
            if feat in obs:
                features[feat].append(obs[feat].flatten())
            else:
                features[feat].append(np.zeros(MAX_NUM_CANDIDATES, dtype=np.int64))

        for feat in GLOBAL_FEATURES:
            if feat in obs:
                features[feat].append(obs[feat].flatten())
            else:
                features[feat].append(np.zeros(1, dtype=np.int64))

        decisions.append(obs[DECISION_KEY].flatten()[0])
        rewards.append(obs.get('reward', 0.0))

    # Stack into arrays
    feature_arrays = {}
    for feat in PER_INSTR_FEATURES + GLOBAL_FEATURES:
        feature_arrays[feat] = np.array(features[feat], dtype=np.float32)

    decision_array = np.array(decisions, dtype=np.int64)
    reward_array = np.array(rewards, dtype=np.float32)

    print(f"Loaded {len(decision_array)} training examples")
    print(f"Reward stats: min={reward_array.min():.3f}, max={reward_array.max():.3f}, mean={reward_array.mean():.3f}")

    return feature_arrays, decision_array, reward_array


# ==============================================================================
# Dataset
# ==============================================================================


class SchedulingDataset(Dataset):
    """PyTorch dataset for scheduling training data."""

    def __init__(self, features: Dict[str, np.ndarray], decisions: np.ndarray, rewards: np.ndarray):
        self.features = {k: torch.from_numpy(v) for k, v in features.items()}
        self.decisions = torch.from_numpy(decisions)
        self.rewards = torch.from_numpy(rewards)

        # Filter valid decisions
        valid_mask = (self.decisions >= 0) & (self.decisions < NUM_OUTPUTS)
        self.valid_indices = torch.where(valid_mask)[0]

        print(f"Dataset: {len(self.valid_indices)} valid examples out of {len(decisions)}")

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
# Model Architecture
# ==============================================================================


class SchedulingPolicy(nn.Module):
    """
    Neural network for instruction scheduling policy.

    Architecture:
    - Per-instruction feature embedding
    - Multi-head self-attention over candidates
    - Global feature concatenation
    - Output head with logits over 512 positions
    """

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

        # Per-instruction embedding
        self.per_instr_embed = nn.Sequential(
            nn.Linear(num_per_instr_features, embedding_dim),
            nn.ReLU(),
            nn.LayerNorm(embedding_dim),
        )

        # Self-attention over candidates
        self.attention = nn.MultiheadAttention(
            embed_dim=embedding_dim,
            num_heads=num_attention_heads,
            dropout=dropout_rate,
            batch_first=True,
        )
        self.attention_norm = nn.LayerNorm(embedding_dim)

        # Feed-forward
        self.ff = nn.Sequential(
            nn.Linear(embedding_dim, ff_dim),
            nn.ReLU(),
            nn.Linear(ff_dim, embedding_dim),
            nn.Dropout(dropout_rate),
        )
        self.ff_norm = nn.LayerNorm(embedding_dim)

        # Output head
        self.output_head = nn.Sequential(
            nn.Linear(embedding_dim * MAX_NUM_CANDIDATES + num_global_features, 256),
            nn.ReLU(),
            nn.Dropout(dropout_rate),
            nn.Linear(256, 128),
            nn.ReLU(),
            nn.Linear(128, NUM_OUTPUTS),
        )

    def forward(self, per_instr: torch.Tensor, global_feats: torch.Tensor) -> torch.Tensor:
        """
        Args:
            per_instr: [batch, num_candidates, num_features]
            global_feats: [batch, num_global_features]

        Returns:
            logits: [batch, num_outputs]
        """
        # Embed per-instruction features
        x = self.per_instr_embed(per_instr)  # [batch, num_candidates, embed_dim]

        # Self-attention
        attn_out, _ = self.attention(x, x, x)
        x = self.attention_norm(x + attn_out)

        # Feed-forward
        ff_out = self.ff(x)
        x = self.ff_norm(x + ff_out)

        # Flatten and concatenate global features
        x = x.flatten(start_dim=1)  # [batch, num_candidates * embed_dim]
        x = torch.cat([x, global_feats], dim=-1)

        # Output logits
        logits = self.output_head(x)

        return logits


# ==============================================================================
# Training
# ==============================================================================


def train_model(
    model: nn.Module,
    train_loader: DataLoader,
    val_loader: DataLoader,
    output_dir: Path,
    epochs: int = 50,
    learning_rate: float = 0.001,
    device: str = "cuda" if torch.cuda.is_available() else "cpu",
):
    """Train the model using behavioral cloning."""
    output_dir.mkdir(parents=True, exist_ok=True)

    model = model.to(device)
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=5, factor=0.5)
    criterion = nn.CrossEntropyLoss()

    best_val_loss = float('inf')
    patience = 10
    patience_counter = 0

    history = {'train_loss': [], 'val_loss': [], 'train_acc': [], 'val_acc': []}

    for epoch in range(epochs):
        # Training
        model.train()
        train_loss = 0.0
        train_correct = 0
        train_total = 0

        for batch in train_loader:
            per_instr = batch['per_instr'].to(device)
            global_feats = batch['global'].to(device)
            decisions = batch['decision'].to(device)

            optimizer.zero_grad()
            logits = model(per_instr, global_feats)
            loss = criterion(logits, decisions)
            loss.backward()
            optimizer.step()

            train_loss += loss.item() * len(decisions)
            train_correct += (logits.argmax(dim=-1) == decisions).sum().item()
            train_total += len(decisions)

        train_loss /= train_total
        train_acc = train_correct / train_total

        # Validation
        model.eval()
        val_loss = 0.0
        val_correct = 0
        val_total = 0

        with torch.no_grad():
            for batch in val_loader:
                per_instr = batch['per_instr'].to(device)
                global_feats = batch['global'].to(device)
                decisions = batch['decision'].to(device)

                logits = model(per_instr, global_feats)
                loss = criterion(logits, decisions)

                val_loss += loss.item() * len(decisions)
                val_correct += (logits.argmax(dim=-1) == decisions).sum().item()
                val_total += len(decisions)

        val_loss /= val_total
        val_acc = val_correct / val_total

        scheduler.step(val_loss)

        history['train_loss'].append(train_loss)
        history['val_loss'].append(val_loss)
        history['train_acc'].append(train_acc)
        history['val_acc'].append(val_acc)

        print(f"Epoch {epoch+1}/{epochs}: "
              f"train_loss={train_loss:.4f}, train_acc={train_acc:.4f}, "
              f"val_loss={val_loss:.4f}, val_acc={val_acc:.4f}")

        # Save best model
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            patience_counter = 0
            torch.save(model.state_dict(), output_dir / 'best_model.pt')
            print(f"  -> New best model saved!")
        else:
            patience_counter += 1
            if patience_counter >= patience:
                print(f"Early stopping at epoch {epoch+1}")
                break

    # Load best model
    model.load_state_dict(torch.load(output_dir / 'best_model.pt'))

    # Save final model
    torch.save(model.state_dict(), output_dir / 'model.pt')
    print(f"Model saved to {output_dir / 'model.pt'}")

    # Export to ONNX
    model.eval()
    dummy_per_instr = torch.randn(1, MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES)).to(device)
    dummy_global = torch.randn(1, len(GLOBAL_FEATURES)).to(device)

    torch.onnx.export(
        model,
        (dummy_per_instr, dummy_global),
        output_dir / 'model.onnx',
        input_names=['per_instr', 'global'],
        output_names=['logits'],
        dynamic_axes={
            'per_instr': {0: 'batch'},
            'global': {0: 'batch'},
            'logits': {0: 'batch'},
        },
    )
    print(f"ONNX model saved to {output_dir / 'model.onnx'}")

    # Save history
    with open(output_dir / 'history.json', 'w') as f:
        json.dump(history, f, indent=2)

    return history


# ==============================================================================
# Main
# ==============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="Train warmstart model for MLGO instruction scheduling (PyTorch)"
    )
    parser.add_argument(
        "--data", "-d", type=Path, required=True,
        help="Directory containing training logs"
    )
    parser.add_argument(
        "--output", "-o", type=Path, required=True,
        help="Output directory for model"
    )
    parser.add_argument(
        "--epochs", type=int, default=50,
        help="Number of training epochs (default: 50)"
    )
    parser.add_argument(
        "--batch-size", type=int, default=32,
        help="Batch size (default: 32)"
    )
    parser.add_argument(
        "--learning-rate", type=float, default=0.001,
        help="Learning rate (default: 0.001)"
    )
    parser.add_argument(
        "--embedding-dim", type=int, default=64,
        help="Embedding dimension (default: 64)"
    )
    parser.add_argument(
        "--val-split", type=float, default=0.1,
        help="Validation split ratio (default: 0.1)"
    )
    args = parser.parse_args()

    # Load data
    print("Loading training data...")
    features, decisions, rewards = load_training_data(args.data)

    # Create dataset
    dataset = SchedulingDataset(features, decisions, rewards)

    # Split into train/val
    val_size = int(len(dataset) * args.val_split)
    train_size = len(dataset) - val_size
    train_dataset, val_dataset = random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size, shuffle=False)

    print(f"Train: {len(train_dataset)}, Val: {len(val_dataset)}")

    # Create model
    print("Creating model...")
    model = SchedulingPolicy(embedding_dim=args.embedding_dim)
    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

    # Train
    print("\nStarting training...")
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")

    history = train_model(
        model,
        train_loader,
        val_loader,
        args.output,
        epochs=args.epochs,
        learning_rate=args.learning_rate,
        device=device,
    )

    # Print final metrics
    print("\nTraining complete!")
    print(f"Best validation loss: {min(history['val_loss']):.4f}")
    print(f"Best validation accuracy: {max(history['val_acc']):.4f}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
