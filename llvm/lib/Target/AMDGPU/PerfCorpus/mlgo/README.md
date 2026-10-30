# MLGO Training Infrastructure for CoExec Scheduler

This directory contains scripts and utilities for training ML models to guide instruction scheduling.

## Directory Structure

```
mlgo/
├── README.md              # This file
├── collect_training_data.py  # Run corpus through compiler, collect logs
├── inspect_log.py            # Inspect TFRecord logs
├── train_warmstart.py        # Behavioral cloning (supervised learning)
├── train_ppo.py              # PPO reinforcement learning
├── models/                   # Saved model checkpoints
│   ├── warmstart/
│   └── ppo/
└── logs/                     # Training logs (TFRecord format)
```

## Quick Start

### 1. Collect Training Data (Heuristic Decisions)

```bash
# Collect trajectories using CoExec heuristic (for warmstart)
python3 collect_training_data.py --output logs/heuristic/

# Collect trajectories using a model
python3 collect_training_data.py --model models/warmstart/model.tflite --output logs/iter_01/
```

### 2. Train Warmstart Model (Behavioral Cloning)

```bash
# Basic training
python3 train_warmstart.py --data logs/heuristic/ --output models/warmstart/

# With custom hyperparameters
python3 train_warmstart.py --data logs/heuristic/ --output models/warmstart/ \
    --epochs 100 --batch-size 64 --learning-rate 0.001

# Resume from checkpoint
python3 train_warmstart.py --data logs/heuristic/ --output models/warmstart/ \
    --checkpoint models/warmstart/checkpoint.keras
```

### 3. Train with PPO

```bash
# Basic PPO training from warmstart
python3 train_ppo.py \
    --warmstart models/warmstart/model.keras \
    --data-dir logs/heuristic/ \
    --output models/ppo/

# With custom hyperparameters
python3 train_ppo.py \
    --warmstart models/warmstart/model.keras \
    --data-dir logs/heuristic/ \
    --output models/ppo/ \
    --iterations 100 --batch-size 64 --ppo-epochs 10
```

### 4. Use Trained Model with llc

```bash
llc --mlsched-model=models/ppo/final/model.tflite \
    --mlsched-training-log=logs/eval/kernel.tfrecord \
    -mtriple=amdgcn-amd-amdhsa -mcpu=gfx1250 \
    kernel.ll -o /dev/null
```

## Feature Specification

The model receives 18 features from MLGCNSchedStrategy:

### Per-Instruction Features (shape: [1, 256])
| Feature | Description |
|---------|-------------|
| `mask` | 1 if position is valid, 0 otherwise |
| `is_top` | 1 if instruction is from Top queue |
| `is_bot` | 1 if instruction is from Bottom queue |
| `pos` | Position in scheduling region (SU->NodeNum) |
| `excess` | Register pressure excess delta |
| `current_max` | Current max register pressure delta |
| `critical_max` | Critical max register pressure delta |
| `su_latency` | Instruction latency |
| `su_height` | Height in dependency graph |
| `su_depth` | Depth in dependency graph |
| `su_succs_left` | Remaining successors |
| `su_preds_left` | Remaining predecessors |
| `su_succs` | Total successors |
| `su_preds` | Total predecessors |

### Global Features (shape: [1])
| Feature | Description |
|---------|-------------|
| `sgpr_critical_limit` | SGPR critical pressure limit |
| `vgpr_critical_limit` | VGPR critical pressure limit |
| `sgpr_excess_limit` | SGPR excess pressure limit |
| `vgpr_excess_limit` | VGPR excess pressure limit |

### Model Output
- **Decision**: `index_to_sched` (int64), where `SchedIndex = 2 * Pos + IsTopNode`
- **Output space**: 512 positions (256 candidates x 2 queues)

### Reward Signal
```
reward = 1.0 - stall_ratio
```
Range: 0-1, higher is better (less stalls).

## Requirements

- Python 3.8+
- TensorFlow 2.x
- TF-Agents (for PPO): `pip install tf-agents`
- LLVM build with TFLite support

## Model Architecture

Both training scripts use a shared architecture:
1. **Per-instruction embedding**: Dense layers to embed 14 features
2. **Self-attention**: Multi-head attention for candidate interactions
3. **Global embedding**: Process 4 global features
4. **Output head**: Logits over 512 positions (masked by validity)

The PPO script adds a value head for the critic network.

## Training Corpus

Located at `../MI450/`:
- AAI: bf16FA, bf16_GEMM, fp4_GEMM, fp8fp4MOE
- Gluon: bf16FA, bf16_GEMM, fp8_FA, fp8_GEMM

## Notes

- All kernels use `-waves-per-eu=1` (WMMA-heavy workloads)
- Static simulator must be enabled with `--amdgpu-enable-static-simulator=1`
- Training logs are in TFRecord format (Protocol Buffers)
- MaxNumInstCandidates = 256 (128 * 2)
- Command line flags: `--mlsched-training-log` and `--mlsched-model`
