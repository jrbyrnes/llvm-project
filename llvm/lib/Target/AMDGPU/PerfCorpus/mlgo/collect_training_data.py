#!/usr/bin/env python3
"""
Collect training data for MLGO-guided instruction scheduling.

This script runs the MI450 kernel corpus through llc with the MLCoExec scheduler,
collecting features, decisions, and rewards into training logs.

The reward (stall ratio) is extracted from the Static Simulator output in the
generated assembly file and saved alongside the training logs for post-hoc
reward assignment during training.

Usage:
    # Collect heuristic decisions (no model)
    python3 collect_training_data.py --output logs/heuristic/

    # Collect with a model
    python3 collect_training_data.py --model models/warmstart.tflite --output logs/iter_01/

    # Verbose mode
    python3 collect_training_data.py --output logs/test/ --verbose
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Dict


SCRIPT_DIR = Path(__file__).parent.resolve()
CORPUS_DIR = SCRIPT_DIR.parent / "MI450"

# Base flags for all compilations
BASE_FLAGS = [
    "-mtriple=amdgcn-amd-amdhsa",
    "-mcpu=gfx1250",
    "--amdgpu-enable-static-simulator=1",
    "--amdgpu-expert-scheduling-mode",
    "--enable-post-misched=0",
    "--amdgpu-sched-strategy=mlcoexec",  # Use ML-augmented CoExec scheduler
]


@dataclass
class KernelResult:
    name: str
    ll_path: Path
    log_path: Optional[Path]
    asm_path: Optional[Path]
    stall_ratio: Optional[float]  # 0.0 to 1.0
    stall_percent: Optional[float]  # 0.0 to 100.0
    ipc: Optional[float]
    total_cycles: Optional[int]
    stall_cycles: Optional[int]
    success: bool
    error: Optional[str] = None
    per_block_stats: List[Dict] = field(default_factory=list)


def find_llc() -> Path:
    """Find llc binary, checking LLC_PATH env var first."""
    if env_path := os.environ.get("LLC_PATH"):
        return Path(env_path)

    # Try relative path from repo root
    repo_root = SCRIPT_DIR.parents[5]  # Up from mlgo/ to llvm-project
    build_llc = repo_root / "build" / "bin" / "llc"
    if build_llc.exists():
        return build_llc

    # Try system llc
    result = subprocess.run(["which", "llc"], capture_output=True, text=True)
    if result.returncode == 0:
        return Path(result.stdout.strip())

    raise RuntimeError("Could not find llc. Set LLC_PATH environment variable.")


def find_kernels() -> List[Path]:
    """Find all .ll files in the corpus."""
    return sorted(CORPUS_DIR.rglob("*.ll"))


def extract_stall_info(asm_content: str) -> Dict:
    """
    Extract stall information from Static Simulator comments in assembly.

    Looks for patterns like:
    - ;;   IPC: 0.15 | Stall ratio: 80.1%
    - ;;   Stall: 20073 cycles (80.1%)
    - ;  Stall: 499 cycles (70%)  (per-block)
    """
    result = {
        'stall_ratio': None,
        'stall_percent': None,
        'ipc': None,
        'total_cycles': None,
        'stall_cycles': None,
        'per_block_stats': []
    }

    # Look for function-level summary: "IPC: X.XX | Stall ratio: YY.Y%"
    ipc_match = re.search(r'IPC:\s*([\d.]+)\s*\|\s*Stall ratio:\s*([\d.]+)%', asm_content)
    if ipc_match:
        result['ipc'] = float(ipc_match.group(1))
        result['stall_percent'] = float(ipc_match.group(2))
        result['stall_ratio'] = result['stall_percent'] / 100.0

    # Look for total stall line: ";;   Stall: NNNN cycles (XX.X%)"
    total_stall_match = re.search(r';;\s+Stall:\s+(\d+)\s+cycles\s+\(([\d.]+)%\)', asm_content)
    if total_stall_match:
        result['stall_cycles'] = int(total_stall_match.group(1))
        if result['stall_percent'] is None:
            result['stall_percent'] = float(total_stall_match.group(2))
            result['stall_ratio'] = result['stall_percent'] / 100.0

    # Extract per-block stats: ";  Stall: NNN cycles (XX%)"
    for match in re.finditer(r';\s+Stall:\s+(\d+)\s+cycles\s+\((\d+)%\)', asm_content):
        result['per_block_stats'].append({
            'stall_cycles': int(match.group(1)),
            'stall_percent': int(match.group(2))
        })

    return result


def run_kernel(
    llc_path: Path,
    ll_path: Path,
    output_dir: Path,
    model_path: Optional[Path] = None,
    verbose: bool = False,
) -> KernelResult:
    """Run a single kernel through llc and collect training data."""
    kernel_name = ll_path.stem
    log_path = output_dir / f"{kernel_name}.tfrecord"
    asm_path = output_dir / f"{kernel_name}.s"

    # Build command - output assembly to capture Static Simulator results
    cmd = [str(llc_path)] + BASE_FLAGS + [
        f"--mlcoexec-training-log={log_path}",
        "-o", str(asm_path),
        str(ll_path),
    ]

    if model_path:
        cmd.append(f"--mlcoexec-model={model_path}")

    if verbose:
        print(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,  # 5 minute timeout
        )

        # Check for errors
        if result.returncode != 0:
            return KernelResult(
                name=kernel_name,
                ll_path=ll_path,
                log_path=None,
                asm_path=None,
                stall_ratio=None,
                stall_percent=None,
                ipc=None,
                total_cycles=None,
                stall_cycles=None,
                success=False,
                error=f"llc failed: {result.stderr[:500]}",
            )

        # Extract stall info from assembly
        stall_info = {'stall_ratio': None, 'stall_percent': None, 'ipc': None,
                      'total_cycles': None, 'stall_cycles': None, 'per_block_stats': []}
        if asm_path.exists():
            asm_content = asm_path.read_text()
            stall_info = extract_stall_info(asm_content)

        return KernelResult(
            name=kernel_name,
            ll_path=ll_path,
            log_path=log_path if log_path.exists() else None,
            asm_path=asm_path if asm_path.exists() else None,
            stall_ratio=stall_info['stall_ratio'],
            stall_percent=stall_info['stall_percent'],
            ipc=stall_info['ipc'],
            total_cycles=stall_info['total_cycles'],
            stall_cycles=stall_info['stall_cycles'],
            success=True,
            per_block_stats=stall_info['per_block_stats'],
        )

    except subprocess.TimeoutExpired:
        return KernelResult(
            name=kernel_name,
            ll_path=ll_path,
            log_path=None,
            asm_path=None,
            stall_ratio=None,
            stall_percent=None,
            ipc=None,
            total_cycles=None,
            stall_cycles=None,
            success=False,
            error="Timeout after 300s",
        )
    except Exception as e:
        return KernelResult(
            name=kernel_name,
            ll_path=ll_path,
            log_path=None,
            asm_path=None,
            stall_ratio=None,
            stall_percent=None,
            ipc=None,
            total_cycles=None,
            stall_cycles=None,
            success=False,
            error=str(e),
        )


def main():
    parser = argparse.ArgumentParser(description="Collect MLGO training data")
    parser.add_argument("--output", "-o", type=Path, required=True,
                        help="Output directory for training logs")
    parser.add_argument("--model", "-m", type=Path, default=None,
                        help="Model to use (if None, uses heuristic fallback)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose output")
    parser.add_argument("--kernel", "-k", type=str, default=None,
                        help="Run only this kernel (substring match)")
    args = parser.parse_args()

    # Find llc
    try:
        llc_path = find_llc()
        print(f"Using llc: {llc_path}")
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Find kernels
    kernels = find_kernels()
    if not kernels:
        print(f"No .ll files found in {CORPUS_DIR}", file=sys.stderr)
        return 1

    # Filter if requested
    if args.kernel:
        kernels = [k for k in kernels if args.kernel in str(k)]

    print(f"Found {len(kernels)} kernel(s)")

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    # Process each kernel
    results = []
    for ll_path in kernels:
        print(f"\nProcessing: {ll_path.name}")
        result = run_kernel(
            llc_path,
            ll_path,
            args.output,
            args.model,
            args.verbose,
        )
        results.append(result)

        if result.success:
            if result.stall_ratio is not None:
                reward = 1.0 - result.stall_ratio
                print(f"  Stall: {result.stall_percent:.1f}% (reward={reward:.3f})")
            else:
                print(f"  Stall: N/A")
            if result.log_path:
                print(f"  Log: {result.log_path}")
            if result.ipc is not None:
                print(f"  IPC: {result.ipc:.2f}")
        else:
            print(f"  FAILED: {result.error}")

    # Summary
    print("\n" + "="*60)
    print("Summary:")
    succeeded = sum(1 for r in results if r.success)
    print(f"  Succeeded: {succeeded}/{len(results)}")

    stall_ratios = [r.stall_ratio for r in results if r.stall_ratio is not None]
    if stall_ratios:
        avg_stall = sum(stall_ratios) / len(stall_ratios) * 100
        avg_reward = 1.0 - (sum(stall_ratios) / len(stall_ratios))
        print(f"  Average stall: {avg_stall:.1f}%")
        print(f"  Average reward: {avg_reward:.3f}")

    # Save results summary with rewards for training
    summary_path = args.output / "summary.json"
    summary_data = []
    for r in results:
        entry = {
            "name": r.name,
            "ll_path": str(r.ll_path),
            "log_path": str(r.log_path) if r.log_path else None,
            "asm_path": str(r.asm_path) if r.asm_path else None,
            "stall_ratio": r.stall_ratio,
            "stall_percent": r.stall_percent,
            "reward": (1.0 - r.stall_ratio) if r.stall_ratio is not None else None,
            "ipc": r.ipc,
            "stall_cycles": r.stall_cycles,
            "success": r.success,
            "error": r.error,
            "per_block_stats": r.per_block_stats,
        }
        summary_data.append(entry)

    with open(summary_path, "w") as f:
        json.dump(summary_data, f, indent=2)
    print(f"\nSummary saved to: {summary_path}")

    # Save rewards in a simple format for easy loading during training
    rewards_path = args.output / "rewards.json"
    rewards_data = {
        r.name: {
            "stall_ratio": r.stall_ratio,
            "reward": (1.0 - r.stall_ratio) if r.stall_ratio is not None else None,
        }
        for r in results if r.success and r.stall_ratio is not None
    }
    with open(rewards_path, "w") as f:
        json.dump(rewards_data, f, indent=2)
    print(f"Rewards saved to: {rewards_path}")

    return 0 if succeeded == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
