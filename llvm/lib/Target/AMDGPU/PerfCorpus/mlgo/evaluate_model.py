#!/usr/bin/env python3
"""
Evaluate trained MLGO scheduling models against heuristic baselines.

This script compares the performance of a trained ML model against either:
- The MLCoExec heuristic fallback (no model)
- The original CoExec scheduler

It runs kernels through both configurations and compares stall percentages
from the Static Simulator output.

Usage:
    # Compare model against heuristic fallback
    python3 evaluate_model.py --model models/ppo/best.tflite --baseline heuristic

    # Compare model against original CoExec
    python3 evaluate_model.py --model models/ppo/best.tflite --baseline coexec

    # Custom corpus path
    python3 evaluate_model.py --model models/best.tflite --corpus ../MI450/

    # Save results to JSON
    python3 evaluate_model.py --model models/best.tflite --output results.json

    # Run specific kernel(s)
    python3 evaluate_model.py --model models/best.tflite --kernel matmul
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Optional, Tuple


SCRIPT_DIR = Path(__file__).parent.resolve()
DEFAULT_CORPUS_DIR = SCRIPT_DIR.parent / "MI450"

# Common compilation flags
COMMON_FLAGS = [
    "-mtriple=amdgcn-amd-amdhsa",
    "-mcpu=gfx1250",
    "--amdgpu-enable-static-simulator=1",
    "--amdgpu-expert-scheduling-mode",
    "--enable-post-misched=0",
]


@dataclass
class EvalResult:
    """Result of evaluating a single kernel."""
    name: str
    ll_path: str
    baseline_stall: Optional[float]
    model_stall: Optional[float]
    delta: Optional[float]  # model - baseline (negative = improvement)
    improvement_pct: Optional[float]  # percentage improvement
    baseline_error: Optional[str] = None
    model_error: Optional[str] = None

    @property
    def improved(self) -> bool:
        """True if model is better than baseline."""
        return self.delta is not None and self.delta < 0

    @property
    def regressed(self) -> bool:
        """True if model is worse than baseline."""
        return self.delta is not None and self.delta > 0


def find_llc() -> Path:
    """Find llc binary, checking LLC_PATH env var first."""
    if env_path := os.environ.get("LLC_PATH"):
        path = Path(env_path)
        if path.exists():
            return path
        raise RuntimeError(f"LLC_PATH set to {env_path} but file does not exist")

    # Try relative path from repo root
    repo_root = SCRIPT_DIR.parents[5]  # Up from mlgo/ to llvm-project
    build_llc = repo_root / "build" / "bin" / "llc"
    if build_llc.exists():
        return build_llc

    # Try system llc
    result = subprocess.run(["which", "llc"], capture_output=True, text=True)
    if result.returncode == 0:
        return Path(result.stdout.strip())

    raise RuntimeError(
        "Could not find llc. Set LLC_PATH environment variable or ensure llc is in PATH."
    )


def find_kernels(corpus_dir: Path) -> List[Path]:
    """Find all .ll files in the corpus directory."""
    if not corpus_dir.exists():
        raise RuntimeError(f"Corpus directory does not exist: {corpus_dir}")
    kernels = sorted(corpus_dir.rglob("*.ll"))
    return kernels


def extract_stall_percent(output: str) -> Optional[float]:
    """Extract stall percentage from static simulator output.

    Looks for pattern: "Stall: N cycles (X.X%)"
    """
    match = re.search(r"Stall:\s*\d+\s*cycles?\s*\(([\d.]+)%\)", output)
    if match:
        return float(match.group(1))
    return None


def run_llc(
    llc_path: Path,
    ll_path: Path,
    flags: List[str],
    timeout: int = 300,
) -> Tuple[Optional[float], Optional[str]]:
    """Run llc and return (stall_percent, error_message)."""
    cmd = [str(llc_path)] + flags + ["-o", "/dev/null", str(ll_path)]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )

        if result.returncode != 0:
            error_msg = result.stderr[:500] if result.stderr else "Unknown error"
            return None, f"llc exit code {result.returncode}: {error_msg}"

        stall_pct = extract_stall_percent(result.stderr)
        return stall_pct, None

    except subprocess.TimeoutExpired:
        return None, f"Timeout after {timeout}s"
    except Exception as e:
        return None, str(e)


def get_baseline_flags(baseline: str) -> List[str]:
    """Get llc flags for the baseline configuration."""
    if baseline == "heuristic":
        # MLCoExec without a model uses heuristic fallback
        return COMMON_FLAGS + ["--amdgpu-sched-strategy=mlcoexec"]
    elif baseline == "coexec":
        # Original CoExec scheduler
        return COMMON_FLAGS + ["--amdgpu-sched-strategy=coexec"]
    else:
        raise ValueError(f"Unknown baseline: {baseline}. Use 'heuristic' or 'coexec'.")


def get_model_flags(model_path: Path) -> List[str]:
    """Get llc flags for model evaluation."""
    return COMMON_FLAGS + [
        "--amdgpu-sched-strategy=mlcoexec",
        f"--mlcoexec-model={model_path}",
    ]


def evaluate_kernel(
    llc_path: Path,
    ll_path: Path,
    baseline_flags: List[str],
    model_flags: List[str],
    verbose: bool = False,
) -> EvalResult:
    """Evaluate a single kernel with both baseline and model."""
    kernel_name = ll_path.stem

    if verbose:
        print(f"  Evaluating: {kernel_name}")

    # Run baseline
    baseline_stall, baseline_error = run_llc(llc_path, ll_path, baseline_flags)

    # Run with model
    model_stall, model_error = run_llc(llc_path, ll_path, model_flags)

    # Calculate delta and improvement
    delta = None
    improvement_pct = None
    if baseline_stall is not None and model_stall is not None:
        delta = model_stall - baseline_stall
        if baseline_stall > 0:
            improvement_pct = -delta / baseline_stall * 100  # positive = improvement

    return EvalResult(
        name=kernel_name,
        ll_path=str(ll_path),
        baseline_stall=baseline_stall,
        model_stall=model_stall,
        delta=delta,
        improvement_pct=improvement_pct,
        baseline_error=baseline_error,
        model_error=model_error,
    )


def evaluate_kernel_worker(args: tuple) -> EvalResult:
    """Worker function for parallel execution."""
    llc_path, ll_path, baseline_flags, model_flags, verbose = args
    return evaluate_kernel(
        Path(llc_path),
        Path(ll_path),
        baseline_flags,
        model_flags,
        verbose,
    )


def print_results_table(results: List[EvalResult], baseline_name: str):
    """Print a formatted results table."""
    # Header
    print()
    print("=" * 100)
    print(f"{'Kernel':<40} {'Baseline':>12} {'Model':>12} {'Delta':>10} {'Improv%':>10}")
    print("=" * 100)

    # Sort by improvement (best first)
    sorted_results = sorted(
        results,
        key=lambda r: r.improvement_pct if r.improvement_pct is not None else float('-inf'),
        reverse=True,
    )

    for r in sorted_results:
        baseline_str = f"{r.baseline_stall:.1f}%" if r.baseline_stall is not None else "ERROR"
        model_str = f"{r.model_stall:.1f}%" if r.model_stall is not None else "ERROR"

        if r.delta is not None:
            delta_str = f"{r.delta:+.1f}pp"
            improv_str = f"{r.improvement_pct:+.1f}%"
            # Color indicator
            if r.improved:
                delta_str = f"{delta_str} [+]"
            elif r.regressed:
                delta_str = f"{delta_str} [-]"
        else:
            delta_str = "N/A"
            improv_str = "N/A"

        # Truncate long kernel names
        name = r.name[:38] + ".." if len(r.name) > 40 else r.name
        print(f"{name:<40} {baseline_str:>12} {model_str:>12} {delta_str:>10} {improv_str:>10}")

    print("=" * 100)


def print_summary(results: List[EvalResult], baseline_name: str):
    """Print summary statistics."""
    valid_results = [r for r in results if r.delta is not None]
    improved = [r for r in valid_results if r.improved]
    regressed = [r for r in valid_results if r.regressed]
    unchanged = [r for r in valid_results if r.delta == 0]

    total = len(results)
    errors = total - len(valid_results)

    print()
    print("Summary Statistics")
    print("-" * 50)
    print(f"  Total kernels:      {total}")
    print(f"  Successfully eval:  {len(valid_results)}")
    print(f"  Errors:             {errors}")
    print()
    print(f"  Improved:           {len(improved)} ({len(improved)/len(valid_results)*100:.1f}%)" if valid_results else "")
    print(f"  Regressed:          {len(regressed)} ({len(regressed)/len(valid_results)*100:.1f}%)" if valid_results else "")
    print(f"  Unchanged:          {len(unchanged)}")

    if valid_results:
        deltas = [r.delta for r in valid_results]
        improvements = [r.improvement_pct for r in valid_results]

        avg_delta = sum(deltas) / len(deltas)
        avg_improvement = sum(improvements) / len(improvements)

        print()
        print(f"  Avg delta:          {avg_delta:+.2f} percentage points")
        print(f"  Avg improvement:    {avg_improvement:+.2f}%")

        # Best and worst
        if improved:
            best = max(improved, key=lambda r: r.improvement_pct)
            print()
            print(f"  Best improvement:   {best.name}")
            print(f"                      {best.baseline_stall:.1f}% -> {best.model_stall:.1f}% ({best.improvement_pct:+.1f}%)")

        if regressed:
            worst = min(regressed, key=lambda r: r.improvement_pct)
            print()
            print(f"  Worst regression:   {worst.name}")
            print(f"                      {worst.baseline_stall:.1f}% -> {worst.model_stall:.1f}% ({worst.improvement_pct:+.1f}%)")

    print()


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate MLGO scheduling model against baseline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --model models/ppo/best.tflite --baseline heuristic
  %(prog)s --model models/best.tflite --baseline coexec --corpus ../MI450/
  %(prog)s --model models/best.tflite --output results.json --verbose
  %(prog)s --model models/best.tflite --kernel matmul --verbose
        """,
    )
    parser.add_argument(
        "--model", "-m",
        type=Path,
        required=True,
        help="Path to the trained TFLite model",
    )
    parser.add_argument(
        "--baseline", "-b",
        choices=["heuristic", "coexec"],
        default="heuristic",
        help="Baseline to compare against (default: heuristic)",
    )
    parser.add_argument(
        "--corpus", "-c",
        type=Path,
        default=DEFAULT_CORPUS_DIR,
        help=f"Path to kernel corpus (default: {DEFAULT_CORPUS_DIR})",
    )
    parser.add_argument(
        "--output", "-o",
        type=Path,
        default=None,
        help="Save results to JSON file",
    )
    parser.add_argument(
        "--kernel", "-k",
        type=str,
        action="append",
        default=None,
        help="Run only kernel(s) matching this pattern (can be repeated)",
    )
    parser.add_argument(
        "--jobs", "-j",
        type=int,
        default=1,
        help="Number of parallel jobs (default: 1)",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output",
    )

    args = parser.parse_args()

    # Validate model path
    if not args.model.exists():
        print(f"Error: Model not found: {args.model}", file=sys.stderr)
        return 1

    # Find llc
    try:
        llc_path = find_llc()
        print(f"Using llc: {llc_path}")
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Find kernels
    try:
        kernels = find_kernels(args.corpus)
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    if not kernels:
        print(f"No .ll files found in {args.corpus}", file=sys.stderr)
        return 1

    # Filter kernels if requested
    if args.kernel:
        filtered = []
        for k in kernels:
            for pattern in args.kernel:
                if pattern in str(k):
                    filtered.append(k)
                    break
        kernels = filtered
        if not kernels:
            print(f"No kernels match pattern(s): {args.kernel}", file=sys.stderr)
            return 1

    print(f"Model: {args.model}")
    print(f"Baseline: {args.baseline}")
    print(f"Corpus: {args.corpus}")
    print(f"Kernels: {len(kernels)}")

    # Get flags
    baseline_flags = get_baseline_flags(args.baseline)
    model_flags = get_model_flags(args.model)

    if args.verbose:
        print(f"\nBaseline flags: {' '.join(baseline_flags)}")
        print(f"Model flags: {' '.join(model_flags)}")

    # Evaluate kernels
    print(f"\nEvaluating {len(kernels)} kernel(s)...")
    results = []

    if args.jobs > 1:
        # Parallel execution
        work_items = [
            (str(llc_path), str(ll_path), baseline_flags, model_flags, args.verbose)
            for ll_path in kernels
        ]

        with ProcessPoolExecutor(max_workers=args.jobs) as executor:
            futures = {executor.submit(evaluate_kernel_worker, item): item for item in work_items}

            for future in as_completed(futures):
                try:
                    result = future.result()
                    results.append(result)
                    if not args.verbose:
                        # Progress indicator
                        indicator = "+" if result.improved else ("-" if result.regressed else ".")
                        print(indicator, end="", flush=True)
                except Exception as e:
                    print(f"\nError: {e}", file=sys.stderr)
        if not args.verbose:
            print()  # Newline after progress
    else:
        # Sequential execution
        for ll_path in kernels:
            result = evaluate_kernel(
                llc_path,
                ll_path,
                baseline_flags,
                model_flags,
                args.verbose,
            )
            results.append(result)

            if not args.verbose:
                # Progress indicator
                indicator = "+" if result.improved else ("-" if result.regressed else ".")
                print(indicator, end="", flush=True)

        if not args.verbose:
            print()  # Newline after progress

    # Print results
    baseline_name = "Heuristic" if args.baseline == "heuristic" else "CoExec"
    print_results_table(results, baseline_name)
    print_summary(results, baseline_name)

    # Save to JSON if requested
    if args.output:
        output_data = {
            "model": str(args.model),
            "baseline": args.baseline,
            "corpus": str(args.corpus),
            "total_kernels": len(results),
            "results": [asdict(r) for r in results],
            "summary": {
                "improved": len([r for r in results if r.improved]),
                "regressed": len([r for r in results if r.regressed]),
                "errors": len([r for r in results if r.delta is None]),
                "avg_delta": sum(r.delta for r in results if r.delta is not None) / len([r for r in results if r.delta is not None]) if any(r.delta is not None for r in results) else None,
                "avg_improvement_pct": sum(r.improvement_pct for r in results if r.improvement_pct is not None) / len([r for r in results if r.improvement_pct is not None]) if any(r.improvement_pct is not None for r in results) else None,
            },
        }

        with open(args.output, "w") as f:
            json.dump(output_data, f, indent=2)
        print(f"Results saved to: {args.output}")

    # Return non-zero if any errors
    errors = len([r for r in results if r.delta is None])
    return 1 if errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
