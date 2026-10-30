#!/usr/bin/env python3
"""
PPO Reinforcement Learning Training for MLGO Instruction Scheduling.

This script trains a policy using Proximal Policy Optimization (PPO) to optimize
instruction scheduling decisions. It starts from a warmstart (behavioral cloning)
model and improves it using the reward signal from the static simulator.

The reward structure is "total reward" - all decisions in a scheduling region
share the final reward (1.0 - stall_ratio).

Usage:
    # Basic PPO training from warmstart
    python3 train_ppo.py --warmstart models/warmstart/model.keras \
        --data-dir logs/ --output models/ppo/

    # With custom hyperparameters
    python3 train_ppo.py --warmstart models/warmstart/model.keras \
        --data-dir logs/ --output models/ppo/ \
        --iterations 100 --batch-size 64 --ppo-epochs 10

    # Continue training from checkpoint
    python3 train_ppo.py --warmstart models/warmstart/model.keras \
        --data-dir logs/ --output models/ppo/ \
        --checkpoint models/ppo/iter_50/
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple, NamedTuple

import numpy as np

# TensorFlow setup
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")

import tensorflow as tf

# Try to import tf_agents, provide helpful error if missing
try:
    import tf_agents
    from tf_agents.agents.ppo import ppo_clip_agent
    from tf_agents.networks import actor_distribution_network
    from tf_agents.networks import value_network
    from tf_agents.policies import py_tf_eager_policy
    from tf_agents.specs import tensor_spec
    from tf_agents.trajectories import trajectory
    from tf_agents.utils import common
except ImportError:
    print("Error: TF-Agents is required for PPO training.")
    print("Install with: pip install tf-agents")
    sys.exit(1)


# ==============================================================================
# Feature Specification (must match MLGCNSchedStrategy.cpp and train_warmstart.py)
# ==============================================================================

MAX_NUM_CANDIDATES = 256
NUM_OUTPUTS = MAX_NUM_CANDIDATES * 2  # 512 positions

PER_INSTR_FEATURES = [
    "mask", "is_top", "is_bot", "pos", "excess", "current_max", "critical_max",
    "su_latency", "su_height", "su_depth", "su_succs_left", "su_preds_left",
    "su_succs", "su_preds",
]
GLOBAL_FEATURES = [
    "sgpr_critical_limit", "vgpr_critical_limit", "sgpr_excess_limit", "vgpr_excess_limit",
]

DECISION_KEY = "index_to_sched"
REWARD_KEY = "reward"


# ==============================================================================
# Trajectory Data Structures
# ==============================================================================


class Transition(NamedTuple):
    """A single (state, action, reward) transition."""
    per_instr: np.ndarray      # [256, 14]
    global_feat: np.ndarray    # [4]
    mask: np.ndarray           # [256]
    action: int                # scalar
    reward: float              # scalar


class Episode(NamedTuple):
    """A sequence of transitions for one scheduling region."""
    transitions: List[Transition]
    final_reward: float


# ==============================================================================
# TFRecord Parsing
# ==============================================================================


def create_feature_description() -> Dict[str, tf.io.FixedLenFeature]:
    """Create feature description for parsing TFRecords."""
    feature_desc = {}

    for feat in PER_INSTR_FEATURES:
        feature_desc[feat] = tf.io.FixedLenFeature(
            [MAX_NUM_CANDIDATES], tf.int64, default_value=[0] * MAX_NUM_CANDIDATES
        )

    for feat in GLOBAL_FEATURES:
        feature_desc[feat] = tf.io.FixedLenFeature([1], tf.int64, default_value=[0])

    feature_desc[DECISION_KEY] = tf.io.FixedLenFeature([1], tf.int64, default_value=[0])
    feature_desc[REWARD_KEY] = tf.io.FixedLenFeature([1], tf.float32, default_value=[0.0])

    return feature_desc


def parse_tfrecord_numpy(serialized_example: bytes) -> Optional[Tuple[Dict, int, float]]:
    """Parse a TFRecord example into numpy arrays."""
    example = tf.train.Example()
    example.ParseFromString(serialized_example)

    features = {}

    # Parse per-instruction features
    per_instr = []
    for feat in PER_INSTR_FEATURES:
        if feat in example.features.feature:
            vals = list(example.features.feature[feat].int64_list.value)
            if len(vals) < MAX_NUM_CANDIDATES:
                vals.extend([0] * (MAX_NUM_CANDIDATES - len(vals)))
            per_instr.append(np.array(vals[:MAX_NUM_CANDIDATES], dtype=np.float32))
        else:
            per_instr.append(np.zeros(MAX_NUM_CANDIDATES, dtype=np.float32))

    features["per_instr"] = np.stack(per_instr, axis=-1)  # [256, 14]

    # Parse global features
    global_feat = []
    for feat in GLOBAL_FEATURES:
        if feat in example.features.feature:
            val = example.features.feature[feat].int64_list.value
            global_feat.append(float(val[0]) if val else 0.0)
        else:
            global_feat.append(0.0)
    features["global"] = np.array(global_feat, dtype=np.float32)  # [4]

    # Mask
    features["mask"] = per_instr[0]  # mask is the first feature

    # Action
    if DECISION_KEY in example.features.feature:
        action_vals = example.features.feature[DECISION_KEY].int64_list.value
        action = int(action_vals[0]) if action_vals else 0
    else:
        return None

    # Reward
    if REWARD_KEY in example.features.feature:
        reward_vals = example.features.feature[REWARD_KEY].float_list.value
        reward = float(reward_vals[0]) if reward_vals else 0.0
    else:
        reward = 0.0

    return features, action, reward


def load_episodes_from_tfrecords(data_dir: Path) -> List[Episode]:
    """Load episodes from TFRecord files.

    MLGO uses a "total reward" structure where:
    - Intermediate steps have reward = 0
    - The final step of each region has the actual reward

    This function groups transitions into episodes based on reward signals.
    """
    tfrecord_files = glob.glob(str(data_dir / "*.tfrecord"))
    if not tfrecord_files:
        raise ValueError(f"No .tfrecord files found in {data_dir}")

    print(f"Loading from {len(tfrecord_files)} TFRecord files...")

    all_episodes = []

    for tfrecord_path in tfrecord_files:
        current_transitions = []

        # Read raw records
        with open(tfrecord_path, "rb") as f:
            while True:
                # Read length (uint64)
                length_bytes = f.read(8)
                if len(length_bytes) < 8:
                    break
                import struct
                length = struct.unpack("<Q", length_bytes)[0]

                # Skip CRC of length
                f.read(4)

                # Read data
                data = f.read(length)
                if len(data) < length:
                    break

                # Skip CRC of data
                f.read(4)

                # Parse the record
                result = parse_tfrecord_numpy(data)
                if result is None:
                    continue

                features, action, reward = result

                transition = Transition(
                    per_instr=features["per_instr"],
                    global_feat=features["global"],
                    mask=features["mask"],
                    action=action,
                    reward=reward,
                )
                current_transitions.append(transition)

                # Non-zero reward indicates end of episode (scheduling region)
                if abs(reward) > 1e-6:
                    if current_transitions:
                        all_episodes.append(Episode(
                            transitions=current_transitions,
                            final_reward=reward,
                        ))
                    current_transitions = []

        # Handle any remaining transitions
        if current_transitions:
            # Use the last transition's reward, or 0 if none
            final_reward = current_transitions[-1].reward
            all_episodes.append(Episode(
                transitions=current_transitions,
                final_reward=final_reward,
            ))

    print(f"Loaded {len(all_episodes)} episodes with {sum(len(e.transitions) for e in all_episodes)} total transitions")
    return all_episodes


# ==============================================================================
# Policy Network (compatible with warmstart model)
# ==============================================================================


class SchedulingPolicyNetwork(tf.keras.Model):
    """Policy network for instruction scheduling.

    This architecture matches train_warmstart.py's InstructionSchedulingModel
    for loading pretrained weights.
    """

    def __init__(
        self,
        embed_dim: int = 64,
        hidden_dim: int = 128,
        num_attention_heads: int = 4,
        num_layers: int = 2,
        dropout_rate: float = 0.1,
        **kwargs,
    ):
        super().__init__(**kwargs)

        self.embed_dim = embed_dim
        self.hidden_dim = hidden_dim

        # Per-instruction feature embedding
        self.instr_embedding = tf.keras.Sequential([
            tf.keras.layers.Dense(embed_dim, activation="relu"),
            tf.keras.layers.LayerNormalization(),
            tf.keras.layers.Dropout(dropout_rate),
            tf.keras.layers.Dense(embed_dim, activation="relu"),
        ], name="instr_embedding")

        # Self-attention layers
        self.attention_layers = []
        for i in range(num_layers):
            self.attention_layers.append(
                tf.keras.layers.MultiHeadAttention(
                    num_heads=num_attention_heads,
                    key_dim=embed_dim // num_attention_heads,
                    dropout=dropout_rate,
                    name=f"attention_{i}",
                )
            )
            self.attention_layers.append(
                tf.keras.layers.LayerNormalization(name=f"norm_{i}")
            )

        # Global feature processing
        self.global_embedding = tf.keras.Sequential([
            tf.keras.layers.Dense(embed_dim, activation="relu"),
            tf.keras.layers.LayerNormalization(),
        ], name="global_embedding")

        # Policy head (outputs action logits)
        self.policy_head = tf.keras.Sequential([
            tf.keras.layers.Dense(hidden_dim, activation="relu"),
            tf.keras.layers.LayerNormalization(),
            tf.keras.layers.Dropout(dropout_rate),
            tf.keras.layers.Dense(hidden_dim, activation="relu"),
            tf.keras.layers.Dense(NUM_OUTPUTS),  # 512 output positions
        ], name="output_head")

        # Value head (for PPO critic)
        self.value_head = tf.keras.Sequential([
            tf.keras.layers.Dense(hidden_dim, activation="relu"),
            tf.keras.layers.Dense(hidden_dim, activation="relu"),
            tf.keras.layers.Dense(1),  # Single value output
        ], name="value_head")

    def call(self, inputs: Dict[str, tf.Tensor], training: bool = False) -> Tuple[tf.Tensor, tf.Tensor]:
        """Forward pass returning both policy logits and value.

        Args:
            inputs: Dictionary with per_instr, global, mask

        Returns:
            logits: [batch, 512] action logits
            value: [batch, 1] state value estimate
        """
        per_instr = inputs["per_instr"]
        global_feat = inputs["global"]
        mask = inputs["mask"]

        # Embed per-instruction features
        x = self.instr_embedding(per_instr, training=training)

        # Attention mask
        attn_mask = mask[:, tf.newaxis, tf.newaxis, :]

        # Self-attention
        for i in range(0, len(self.attention_layers), 2):
            attn_layer = self.attention_layers[i]
            norm_layer = self.attention_layers[i + 1]
            attn_out = attn_layer(query=x, key=x, value=x, attention_mask=attn_mask, training=training)
            x = norm_layer(x + attn_out)

        # Pool
        mask_expanded = mask[:, :, tf.newaxis]
        pooled = tf.reduce_sum(x * mask_expanded, axis=1)
        mask_sum = tf.reduce_sum(mask, axis=1, keepdims=True) + 1e-8
        pooled = pooled / mask_sum

        # Global embedding
        global_embed = self.global_embedding(global_feat, training=training)

        # Combine
        combined = tf.concat([pooled, global_embed], axis=-1)

        # Policy and value outputs
        logits = self.policy_head(combined, training=training)
        value = self.value_head(combined, training=training)

        # Apply mask to logits
        mask_expanded = tf.repeat(mask, 2, axis=1)
        masked_logits = logits + (1.0 - mask_expanded) * (-1e9)

        return masked_logits, value

    def get_policy_logits(self, inputs: Dict[str, tf.Tensor], training: bool = False) -> tf.Tensor:
        """Get just policy logits (for action selection)."""
        logits, _ = self(inputs, training=training)
        return logits

    def get_value(self, inputs: Dict[str, tf.Tensor], training: bool = False) -> tf.Tensor:
        """Get just value estimate."""
        _, value = self(inputs, training=training)
        return value


# ==============================================================================
# PPO Training Loop
# ==============================================================================


class PPOTrainer:
    """PPO trainer for instruction scheduling."""

    def __init__(
        self,
        policy_network: SchedulingPolicyNetwork,
        learning_rate: float = 1e-4,
        clip_ratio: float = 0.2,
        entropy_coef: float = 0.01,
        value_coef: float = 0.5,
        max_grad_norm: float = 0.5,
        gamma: float = 0.99,
        gae_lambda: float = 0.95,
    ):
        self.policy = policy_network
        self.optimizer = tf.keras.optimizers.Adam(learning_rate=learning_rate)

        self.clip_ratio = clip_ratio
        self.entropy_coef = entropy_coef
        self.value_coef = value_coef
        self.max_grad_norm = max_grad_norm
        self.gamma = gamma
        self.gae_lambda = gae_lambda

    def compute_advantages(
        self,
        rewards: List[float],
        values: List[float],
        dones: List[bool],
    ) -> Tuple[np.ndarray, np.ndarray]:
        """Compute GAE advantages and returns."""
        advantages = np.zeros(len(rewards), dtype=np.float32)
        returns = np.zeros(len(rewards), dtype=np.float32)

        last_advantage = 0.0
        last_return = 0.0

        for t in reversed(range(len(rewards))):
            if t == len(rewards) - 1 or dones[t]:
                next_value = 0.0
            else:
                next_value = values[t + 1]

            delta = rewards[t] + self.gamma * next_value - values[t]
            advantages[t] = delta + self.gamma * self.gae_lambda * (1 - dones[t]) * last_advantage
            returns[t] = rewards[t] + self.gamma * (1 - dones[t]) * last_return

            last_advantage = advantages[t]
            last_return = returns[t]

        return advantages, returns

    @tf.function
    def train_step(
        self,
        per_instr_batch: tf.Tensor,
        global_batch: tf.Tensor,
        mask_batch: tf.Tensor,
        actions_batch: tf.Tensor,
        old_log_probs_batch: tf.Tensor,
        advantages_batch: tf.Tensor,
        returns_batch: tf.Tensor,
    ) -> Dict[str, tf.Tensor]:
        """Single PPO training step."""
        with tf.GradientTape() as tape:
            inputs = {
                "per_instr": per_instr_batch,
                "global": global_batch,
                "mask": mask_batch,
            }

            logits, values = self.policy(inputs, training=True)
            values = tf.squeeze(values, axis=-1)

            # Compute log probabilities
            log_probs = tf.nn.log_softmax(logits, axis=-1)
            action_log_probs = tf.gather(log_probs, actions_batch, batch_dims=1)

            # Policy loss (PPO clipped objective)
            ratio = tf.exp(action_log_probs - old_log_probs_batch)
            clipped_ratio = tf.clip_by_value(ratio, 1 - self.clip_ratio, 1 + self.clip_ratio)
            policy_loss = -tf.reduce_mean(
                tf.minimum(ratio * advantages_batch, clipped_ratio * advantages_batch)
            )

            # Value loss
            value_loss = tf.reduce_mean(tf.square(values - returns_batch))

            # Entropy bonus
            probs = tf.nn.softmax(logits, axis=-1)
            entropy = -tf.reduce_sum(probs * log_probs, axis=-1)
            entropy_loss = -tf.reduce_mean(entropy)

            # Total loss
            total_loss = policy_loss + self.value_coef * value_loss + self.entropy_coef * entropy_loss

        # Compute and apply gradients
        gradients = tape.gradient(total_loss, self.policy.trainable_variables)
        if self.max_grad_norm is not None:
            gradients, _ = tf.clip_by_global_norm(gradients, self.max_grad_norm)
        self.optimizer.apply_gradients(zip(gradients, self.policy.trainable_variables))

        return {
            "total_loss": total_loss,
            "policy_loss": policy_loss,
            "value_loss": value_loss,
            "entropy": -entropy_loss,
            "approx_kl": tf.reduce_mean(old_log_probs_batch - action_log_probs),
        }

    def collect_rollout_data(self, episodes: List[Episode]) -> Dict[str, np.ndarray]:
        """Process episodes into training data with advantages."""
        all_per_instr = []
        all_global = []
        all_mask = []
        all_actions = []
        all_rewards = []
        all_values = []
        all_dones = []

        for episode in episodes:
            episode_values = []

            for i, trans in enumerate(episode.transitions):
                all_per_instr.append(trans.per_instr)
                all_global.append(trans.global_feat)
                all_mask.append(trans.mask)
                all_actions.append(trans.action)

                # Use final reward for all transitions in episode (total reward)
                # Alternatively, could use trans.reward which is 0 for intermediate steps
                if i == len(episode.transitions) - 1:
                    all_rewards.append(episode.final_reward)
                    all_dones.append(True)
                else:
                    all_rewards.append(0.0)
                    all_dones.append(False)

                # Compute value estimate
                inputs = {
                    "per_instr": tf.expand_dims(trans.per_instr, 0),
                    "global": tf.expand_dims(trans.global_feat, 0),
                    "mask": tf.expand_dims(trans.mask, 0),
                }
                value = self.policy.get_value(inputs, training=False).numpy()[0, 0]
                episode_values.append(value)

            all_values.extend(episode_values)

        # Compute advantages
        advantages, returns = self.compute_advantages(all_rewards, all_values, all_dones)

        # Compute old log probs
        per_instr_array = np.stack(all_per_instr)
        global_array = np.stack(all_global)
        mask_array = np.stack(all_mask)
        actions_array = np.array(all_actions, dtype=np.int32)

        inputs = {
            "per_instr": tf.constant(per_instr_array, dtype=tf.float32),
            "global": tf.constant(global_array, dtype=tf.float32),
            "mask": tf.constant(mask_array, dtype=tf.float32),
        }
        logits = self.policy.get_policy_logits(inputs, training=False)
        log_probs = tf.nn.log_softmax(logits, axis=-1).numpy()
        old_log_probs = np.array([log_probs[i, actions_array[i]] for i in range(len(actions_array))])

        # Normalize advantages
        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

        return {
            "per_instr": per_instr_array,
            "global": global_array,
            "mask": mask_array,
            "actions": actions_array,
            "old_log_probs": old_log_probs.astype(np.float32),
            "advantages": advantages,
            "returns": returns,
        }


# ==============================================================================
# TFLite Conversion
# ==============================================================================


def convert_policy_to_tflite(policy: SchedulingPolicyNetwork, output_path: Path) -> None:
    """Convert trained policy to TFLite for LLVM inference."""

    @tf.function(input_signature=[
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="mask"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="is_top"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="is_bot"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="pos"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="excess"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="current_max"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="critical_max"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_latency"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_height"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_depth"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_succs_left"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_preds_left"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_succs"),
        tf.TensorSpec(shape=[1, MAX_NUM_CANDIDATES], dtype=tf.int64, name="su_preds"),
        tf.TensorSpec(shape=[1], dtype=tf.int64, name="sgpr_critical_limit"),
        tf.TensorSpec(shape=[1], dtype=tf.int64, name="vgpr_critical_limit"),
        tf.TensorSpec(shape=[1], dtype=tf.int64, name="sgpr_excess_limit"),
        tf.TensorSpec(shape=[1], dtype=tf.int64, name="vgpr_excess_limit"),
    ])
    def inference_fn(
        mask, is_top, is_bot, pos, excess, current_max, critical_max,
        su_latency, su_height, su_depth, su_succs_left, su_preds_left, su_succs, su_preds,
        sgpr_critical_limit, vgpr_critical_limit, sgpr_excess_limit, vgpr_excess_limit,
    ):
        per_instr = tf.stack([
            tf.cast(mask[0], tf.float32),
            tf.cast(is_top[0], tf.float32),
            tf.cast(is_bot[0], tf.float32),
            tf.cast(pos[0], tf.float32),
            tf.cast(excess[0], tf.float32),
            tf.cast(current_max[0], tf.float32),
            tf.cast(critical_max[0], tf.float32),
            tf.cast(su_latency[0], tf.float32),
            tf.cast(su_height[0], tf.float32),
            tf.cast(su_depth[0], tf.float32),
            tf.cast(su_succs_left[0], tf.float32),
            tf.cast(su_preds_left[0], tf.float32),
            tf.cast(su_succs[0], tf.float32),
            tf.cast(su_preds[0], tf.float32),
        ], axis=-1)
        per_instr = tf.expand_dims(per_instr, 0)

        global_feat = tf.stack([
            tf.cast(sgpr_critical_limit[0], tf.float32),
            tf.cast(vgpr_critical_limit[0], tf.float32),
            tf.cast(sgpr_excess_limit[0], tf.float32),
            tf.cast(vgpr_excess_limit[0], tf.float32),
        ], axis=0)
        global_feat = tf.expand_dims(global_feat, 0)

        mask_float = tf.cast(mask, tf.float32)

        inputs = {
            "per_instr": per_instr,
            "global": global_feat,
            "mask": mask_float,
        }

        logits = policy.get_policy_logits(inputs, training=False)
        decision = tf.argmax(logits, axis=-1, output_type=tf.int64)
        return {"index_to_sched": decision}

    concrete_func = inference_fn.get_concrete_function()

    converter = tf.lite.TFLiteConverter.from_concrete_functions([concrete_func])
    converter.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS,
        tf.lite.OpsSet.SELECT_TF_OPS,
    ]
    converter._experimental_lower_tensor_list_ops = False

    tflite_model = converter.convert()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(tflite_model)

    print(f"TFLite model saved to: {output_path}")
    print(f"Model size: {len(tflite_model) / 1024:.1f} KB")


# ==============================================================================
# Main Training
# ==============================================================================


def train_ppo(
    warmstart_path: Path,
    data_dir: Path,
    output_dir: Path,
    iterations: int = 100,
    batch_size: int = 64,
    ppo_epochs: int = 10,
    learning_rate: float = 1e-4,
    clip_ratio: float = 0.2,
    save_interval: int = 10,
    embed_dim: int = 64,
    hidden_dim: int = 128,
) -> None:
    """Train policy with PPO."""
    print("=" * 60)
    print("MLGO Instruction Scheduling - PPO Training")
    print("=" * 60)
    print(f"Warmstart model: {warmstart_path}")
    print(f"Data directory: {data_dir}")
    print(f"Output directory: {output_dir}")
    print(f"Iterations: {iterations}")
    print(f"Batch size: {batch_size}")
    print(f"PPO epochs per iteration: {ppo_epochs}")
    print()

    output_dir.mkdir(parents=True, exist_ok=True)

    # Create policy network
    print("Creating policy network...")
    policy = SchedulingPolicyNetwork(
        embed_dim=embed_dim,
        hidden_dim=hidden_dim,
    )

    # Build model
    example_inputs = {
        "per_instr": tf.zeros((1, MAX_NUM_CANDIDATES, len(PER_INSTR_FEATURES))),
        "global": tf.zeros((1, len(GLOBAL_FEATURES))),
        "mask": tf.zeros((1, MAX_NUM_CANDIDATES)),
    }
    policy(example_inputs)

    # Load warmstart weights
    if warmstart_path.suffix == ".keras":
        print(f"Loading warmstart weights from {warmstart_path}")
        warmstart_model = tf.keras.models.load_model(warmstart_path)
        # Copy weights for matching layers
        for layer in policy.layers:
            for ws_layer in warmstart_model.layers:
                if layer.name == ws_layer.name:
                    try:
                        layer.set_weights(ws_layer.get_weights())
                        print(f"  Loaded weights for: {layer.name}")
                    except ValueError:
                        print(f"  Weight shape mismatch for: {layer.name}")
    else:
        print(f"Warning: Expected .keras file for warmstart, got {warmstart_path}")

    # Create trainer
    trainer = PPOTrainer(
        policy,
        learning_rate=learning_rate,
        clip_ratio=clip_ratio,
    )

    # Load episodes from data
    print("\nLoading training data...")
    episodes = load_episodes_from_tfrecords(data_dir)

    if not episodes:
        print("Error: No episodes loaded!")
        return

    # Training metrics
    training_history = {
        "iteration": [],
        "total_loss": [],
        "policy_loss": [],
        "value_loss": [],
        "entropy": [],
        "mean_reward": [],
    }

    print(f"\nStarting PPO training for {iterations} iterations...")
    print()

    for iteration in range(1, iterations + 1):
        # Collect rollout data with current policy
        rollout_data = trainer.collect_rollout_data(episodes)

        n_samples = len(rollout_data["actions"])
        indices = np.arange(n_samples)

        # PPO update epochs
        epoch_losses = []
        for epoch in range(ppo_epochs):
            np.random.shuffle(indices)

            for start in range(0, n_samples, batch_size):
                end = min(start + batch_size, n_samples)
                batch_indices = indices[start:end]

                metrics = trainer.train_step(
                    per_instr_batch=tf.constant(rollout_data["per_instr"][batch_indices]),
                    global_batch=tf.constant(rollout_data["global"][batch_indices]),
                    mask_batch=tf.constant(rollout_data["mask"][batch_indices]),
                    actions_batch=tf.constant(rollout_data["actions"][batch_indices]),
                    old_log_probs_batch=tf.constant(rollout_data["old_log_probs"][batch_indices]),
                    advantages_batch=tf.constant(rollout_data["advantages"][batch_indices]),
                    returns_batch=tf.constant(rollout_data["returns"][batch_indices]),
                )
                epoch_losses.append(metrics["total_loss"].numpy())

        # Compute episode rewards
        mean_reward = np.mean([e.final_reward for e in episodes])

        # Log metrics
        avg_loss = np.mean(epoch_losses)
        training_history["iteration"].append(iteration)
        training_history["total_loss"].append(float(avg_loss))
        training_history["policy_loss"].append(float(metrics["policy_loss"].numpy()))
        training_history["value_loss"].append(float(metrics["value_loss"].numpy()))
        training_history["entropy"].append(float(metrics["entropy"].numpy()))
        training_history["mean_reward"].append(float(mean_reward))

        print(f"Iteration {iteration:4d} | Loss: {avg_loss:.4f} | "
              f"Policy: {metrics['policy_loss']:.4f} | Value: {metrics['value_loss']:.4f} | "
              f"Entropy: {metrics['entropy']:.4f} | Reward: {mean_reward:.4f}")

        # Save checkpoint
        if iteration % save_interval == 0:
            checkpoint_dir = output_dir / f"iter_{iteration:04d}"
            checkpoint_dir.mkdir(parents=True, exist_ok=True)

            policy.save_weights(str(checkpoint_dir / "policy_weights"))
            convert_policy_to_tflite(policy, checkpoint_dir / "model.tflite")

            # Save history
            with open(checkpoint_dir / "training_history.json", "w") as f:
                json.dump(training_history, f, indent=2)

            print(f"  -> Checkpoint saved to {checkpoint_dir}")

    # Save final model
    print("\n" + "=" * 60)
    print("Training Complete!")
    print("=" * 60)

    final_dir = output_dir / "final"
    final_dir.mkdir(parents=True, exist_ok=True)

    policy.save_weights(str(final_dir / "policy_weights"))
    convert_policy_to_tflite(policy, final_dir / "model.tflite")

    with open(final_dir / "training_history.json", "w") as f:
        json.dump(training_history, f, indent=2)

    print(f"\nFinal model saved to: {final_dir / 'model.tflite'}")
    print(f"Final mean reward: {training_history['mean_reward'][-1]:.4f}")
    print("\nTo use with llc:")
    print(f"  llc --mlsched-model={final_dir / 'model.tflite'} ...")


def main():
    parser = argparse.ArgumentParser(
        description="Train PPO policy for MLGO instruction scheduling"
    )
    parser.add_argument(
        "--warmstart", "-w", type=Path, required=True,
        help="Path to warmstart model (.keras file from train_warmstart.py)"
    )
    parser.add_argument(
        "--data-dir", "-d", type=Path, required=True,
        help="Directory containing TFRecord training logs"
    )
    parser.add_argument(
        "--output", "-o", type=Path, required=True,
        help="Output directory for checkpoints and final model"
    )
    parser.add_argument(
        "--iterations", "-i", type=int, default=100,
        help="Number of PPO iterations (default: 100)"
    )
    parser.add_argument(
        "--batch-size", "-b", type=int, default=64,
        help="Training batch size (default: 64)"
    )
    parser.add_argument(
        "--ppo-epochs", type=int, default=10,
        help="PPO epochs per iteration (default: 10)"
    )
    parser.add_argument(
        "--learning-rate", "-lr", type=float, default=1e-4,
        help="Learning rate (default: 0.0001)"
    )
    parser.add_argument(
        "--clip-ratio", type=float, default=0.2,
        help="PPO clip ratio (default: 0.2)"
    )
    parser.add_argument(
        "--save-interval", type=int, default=10,
        help="Save checkpoint every N iterations (default: 10)"
    )
    parser.add_argument(
        "--embed-dim", type=int, default=64,
        help="Embedding dimension (default: 64)"
    )
    parser.add_argument(
        "--hidden-dim", type=int, default=128,
        help="Hidden layer dimension (default: 128)"
    )
    args = parser.parse_args()

    train_ppo(
        warmstart_path=args.warmstart,
        data_dir=args.data_dir,
        output_dir=args.output,
        iterations=args.iterations,
        batch_size=args.batch_size,
        ppo_epochs=args.ppo_epochs,
        learning_rate=args.learning_rate,
        clip_ratio=args.clip_ratio,
        save_interval=args.save_interval,
        embed_dim=args.embed_dim,
        hidden_dim=args.hidden_dim,
    )


if __name__ == "__main__":
    sys.exit(main() or 0)
