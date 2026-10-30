#!/usr/bin/env python3
"""
Behavioral Cloning (Warmstart) Training for MLGO Instruction Scheduling.

This script trains a neural network to imitate the heuristic scheduler's decisions
using supervised learning on collected LLVM MLGO logs.

Usage:
    # Basic training
    python3 train_warmstart.py --data logs/heuristic/ --output models/warmstart/

    # With custom hyperparameters
    python3 train_warmstart.py --data logs/heuristic/ --output models/warmstart/ \
        --epochs 100 --batch-size 64 --learning-rate 0.001
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import struct
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

import numpy as np

# Ensure TensorFlow doesn't use too much memory
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")

import tensorflow as tf


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
# LLVM MLGO Log Parsing
# ==============================================================================


def parse_llvm_log(log_path: Path) -> List[Dict[str, Any]]:
    """
    Parse LLVM MLGO log format.

    The format is:
    - Line 1: JSON header with feature specs and score spec
    - Subsequent lines: {"observation": N} followed by binary data, or {"outcome": N}

    Each observation contains:
    - Feature tensors (int64 arrays)
    - Decision index (int64)
    - Reward (float32)
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

        score_spec = header.get('score', {})

        # Calculate sizes
        feature_sizes = {}
        for name, spec in feature_specs.items():
            size = 1
            for dim in spec['shape']:
                size *= dim
            if 'int64' in spec['type']:
                feature_sizes[name] = size * 8  # 8 bytes per int64
            elif 'float' in spec['type']:
                feature_sizes[name] = size * 4  # 4 bytes per float
            else:
                feature_sizes[name] = size * 8  # default to int64

        # Total bytes per observation
        total_feature_bytes = sum(feature_sizes.values())
        reward_bytes = 4  # float32

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
                    observations.append(obs_data)

            elif 'outcome' in line_json:
                # Read reward
                raw = f.read(4)
                if len(raw) >= 4 and observations:
                    reward = struct.unpack('<f', raw)[0]
                    observations[-1]['reward'] = reward

    return observations


def load_training_data(data_dir: Path) -> Tuple[Dict[str, np.ndarray], np.ndarray]:
    """Load all training data from a directory of log files."""
    all_observations = []

    # Find all log files
    log_files = list(data_dir.glob("*.tfrecord")) + list(data_dir.glob("*.log"))

    if not log_files:
        raise ValueError(f"No log files found in {data_dir}")

    print(f"Found {len(log_files)} log file(s)")

    for log_file in log_files:
        print(f"  Loading: {log_file.name}...")
        observations = parse_llvm_log(log_file)
        print(f"    {len(observations)} observations")
        all_observations.extend(observations)

    if not all_observations:
        raise ValueError("No observations loaded from any log file")

    print(f"Total observations: {len(all_observations)}")

    # Convert to arrays
    features = {feat: [] for feat in PER_INSTR_FEATURES + GLOBAL_FEATURES}
    decisions = []

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

    # Stack into arrays
    feature_arrays = {}
    for feat in PER_INSTR_FEATURES + GLOBAL_FEATURES:
        feature_arrays[feat] = np.array(features[feat], dtype=np.float32)

    decision_array = np.array(decisions, dtype=np.int64)

    print(f"Loaded {len(decision_array)} training examples")

    return feature_arrays, decision_array


# ==============================================================================
# Model Architecture
# ==============================================================================


def create_model(
    embedding_dim: int = 64,
    num_attention_heads: int = 4,
    ff_dim: int = 128,
    dropout_rate: float = 0.1,
) -> tf.keras.Model:
    """
    Create the scheduling policy network.

    Architecture:
    - Per-instruction feature embedding
    - Multi-head self-attention over candidates
    - Global feature concatenation
    - Output head with logits over 512 positions
    """
    # Per-instruction feature inputs
    per_instr_inputs = {}
    for feat in PER_INSTR_FEATURES:
        per_instr_inputs[feat] = tf.keras.Input(
            shape=(MAX_NUM_CANDIDATES,), name=feat, dtype=tf.float32
        )

    # Global feature inputs
    global_inputs = {}
    for feat in GLOBAL_FEATURES:
        global_inputs[feat] = tf.keras.Input(
            shape=(1,), name=feat, dtype=tf.float32
        )

    # Stack per-instruction features: [batch, num_candidates, num_features]
    per_instr_stack = tf.stack(
        [per_instr_inputs[f] for f in PER_INSTR_FEATURES], axis=-1
    )

    # Embed per-instruction features
    x = tf.keras.layers.Dense(embedding_dim, activation='relu')(per_instr_stack)
    x = tf.keras.layers.LayerNormalization()(x)

    # Self-attention over candidates
    attention_output = tf.keras.layers.MultiHeadAttention(
        num_heads=num_attention_heads,
        key_dim=embedding_dim // num_attention_heads,
        dropout=dropout_rate,
    )(x, x)
    x = tf.keras.layers.Add()([x, attention_output])
    x = tf.keras.layers.LayerNormalization()(x)

    # Feed-forward
    ff_output = tf.keras.layers.Dense(ff_dim, activation='relu')(x)
    ff_output = tf.keras.layers.Dense(embedding_dim)(ff_output)
    ff_output = tf.keras.layers.Dropout(dropout_rate)(ff_output)
    x = tf.keras.layers.Add()([x, ff_output])
    x = tf.keras.layers.LayerNormalization()(x)

    # Flatten candidate embeddings
    x = tf.keras.layers.Flatten()(x)

    # Concatenate global features
    global_concat = tf.keras.layers.Concatenate()(
        [global_inputs[f] for f in GLOBAL_FEATURES]
    )
    x = tf.keras.layers.Concatenate()([x, global_concat])

    # Output head
    x = tf.keras.layers.Dense(256, activation='relu')(x)
    x = tf.keras.layers.Dropout(dropout_rate)(x)
    x = tf.keras.layers.Dense(128, activation='relu')(x)

    # Output logits over 512 positions (256 candidates * 2 queues)
    outputs = tf.keras.layers.Dense(NUM_OUTPUTS, name='logits')(x)

    # Create model
    all_inputs = {**per_instr_inputs, **global_inputs}
    model = tf.keras.Model(inputs=all_inputs, outputs=outputs, name='sched_policy')

    return model


# ==============================================================================
# Training
# ==============================================================================


def train_model(
    model: tf.keras.Model,
    features: Dict[str, np.ndarray],
    decisions: np.ndarray,
    output_dir: Path,
    epochs: int = 50,
    batch_size: int = 32,
    learning_rate: float = 0.001,
    validation_split: float = 0.1,
):
    """Train the model using behavioral cloning."""
    output_dir.mkdir(parents=True, exist_ok=True)

    # Compile model
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=learning_rate),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
        metrics=['accuracy'],
    )

    # Callbacks
    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor='val_loss',
            patience=10,
            restore_best_weights=True,
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=5,
            min_lr=1e-6,
        ),
        tf.keras.callbacks.ModelCheckpoint(
            output_dir / 'checkpoint.keras',
            save_best_only=True,
            monitor='val_loss',
        ),
        tf.keras.callbacks.TensorBoard(
            log_dir=output_dir / 'tensorboard',
        ),
    ]

    # Create mask for valid positions
    mask = features['mask']

    # Filter out examples where decision is invalid
    valid_mask = (decisions >= 0) & (decisions < NUM_OUTPUTS)

    # Apply mask to select valid samples
    filtered_features = {k: v[valid_mask] for k, v in features.items()}
    filtered_decisions = decisions[valid_mask]

    print(f"Training on {len(filtered_decisions)} valid examples")

    # Train
    history = model.fit(
        filtered_features,
        filtered_decisions,
        epochs=epochs,
        batch_size=batch_size,
        validation_split=validation_split,
        callbacks=callbacks,
        verbose=1,
    )

    # Save final model
    model.save(output_dir / 'model.keras')
    print(f"Model saved to {output_dir / 'model.keras'}")

    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS,
        tf.lite.OpsSet.SELECT_TF_OPS,
    ]
    tflite_model = converter.convert()

    tflite_path = output_dir / 'model.tflite'
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    print(f"TFLite model saved to {tflite_path}")

    return history


# ==============================================================================
# Main
# ==============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="Train warmstart model for MLGO instruction scheduling"
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
        "--checkpoint", type=Path, default=None,
        help="Resume from checkpoint"
    )
    args = parser.parse_args()

    # Load data
    print("Loading training data...")
    features, decisions = load_training_data(args.data)

    # Create or load model
    if args.checkpoint and args.checkpoint.exists():
        print(f"Loading checkpoint from {args.checkpoint}")
        model = tf.keras.models.load_model(args.checkpoint)
    else:
        print("Creating new model...")
        model = create_model(embedding_dim=args.embedding_dim)

    model.summary()

    # Train
    print("\nStarting training...")
    history = train_model(
        model,
        features,
        decisions,
        args.output,
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.learning_rate,
    )

    # Print final metrics
    print("\nTraining complete!")
    print(f"Final training accuracy: {history.history['accuracy'][-1]:.4f}")
    if 'val_accuracy' in history.history:
        print(f"Final validation accuracy: {history.history['val_accuracy'][-1]:.4f}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
