#!/usr/bin/env python3
"""
Performance regression testing for AMDGPU static simulator warm cycle counts.

Usage:
    ./perf_test.py                              # Run tests against baseline.json
    ./perf_test.py --report                     # Write PERF_REPORT.md: Off vs On+tuned per kernel
    ./perf_test.py --sched-only                 # Run with --amdgpu-static-sim-measure-sched=1 against sched_baseline.json
    ./perf_test.py --migration-target           # Use note.txt warm cycles as baseline
    ./perf_test.py --sched-only --migration-target  # Use Sched Perf entries from note.txt
    ./perf_test.py --update                     # Update both baseline.json and sched_baseline.json
    ./perf_test.py --update-on-pass             # Update both baselines only if all tests pass
    ./perf_test.py --verbose                    # Show detailed output
    ./perf_test.py --llc-flag=--foo=1 --llc-flag=--bar  # Pass extra llc flags to every kernel run
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple, Dict


@dataclass
class TestResult:
    name: str
    warm_cycles: Optional[int]
    baseline_cycles: Optional[int]
    passed: bool
    error: Optional[str] = None

    @property
    def delta(self) -> Optional[int]:
        if self.warm_cycles is not None and self.baseline_cycles is not None:
            return self.warm_cycles - self.baseline_cycles
        return None

    @property
    def delta_percent(self) -> Optional[float]:
        if self.delta is not None and self.baseline_cycles and self.baseline_cycles > 0:
            return (self.delta / self.baseline_cycles) * 100
        return None


SCRIPT_DIR = Path(__file__).parent.resolve()
BASELINE_FILE = SCRIPT_DIR / "baseline.json"
SCHED_BASELINE_FILE = SCRIPT_DIR / "sched_baseline.json"
REPORT_FILE = SCRIPT_DIR / "PERF_REPORT.md"

# Flags shared by every configuration. These are the environment the corpus is
# measured in, not scheduler tuning: the static simulator that produces the warm
# cycle counts, expert scheduling mode, disabled post-RA scheduling, and fast FP
# contraction (which changes the IR lowering all configs share).
BASE_FLAGS = [
    "-mtriple=amdgcn-amd-amdhsa",
    "-mcpu=gfx1250",
    "--amdgpu-enable-static-simulator=1",
    "--fp-contract=fast",
    "--amdgpu-expert-scheduling-mode",
    "--enable-post-misched=0",
]

# Flags that turn the CoExec scheduler on.
COEXEC_FLAGS = [
    "--amdgpu-sched-strategy=coexec",
    "--amdgpu-anti-hints-for-va-vdst",
]


@dataclass
class Config:
    """A named llc flag configuration measured by the report."""
    key: str
    label: str
    flags: List[str]
    use_note_flags: bool  # append the per-test Flags section from note.txt


# Off  : base only, default scheduler, no anti-hints.
# On   : base + CoExec scheduler + anti-hints.
# Tuned: On + the per-test tuning flags recorded in each note.txt Flags section.
CONFIG_OFF = Config("off", "Off", list(BASE_FLAGS), use_note_flags=False)
CONFIG_ON = Config("on", "On", BASE_FLAGS + COEXEC_FLAGS, use_note_flags=False)
CONFIG_TUNED = Config("tuned", "On+tuned", BASE_FLAGS + COEXEC_FLAGS, use_note_flags=True)

# Columns of the perf report, left to right.
REPORT_CONFIGS = [CONFIG_OFF, CONFIG_TUNED]

# The regression gate and baseline files measure the fully-enabled configuration,
# which is what the baked-in flags historically produced.
REGRESSION_CONFIG = CONFIG_TUNED

# Pattern to match innermost loop warm cycles
# Format: ;=== Block (loop): Cold=XXXXcyc Warm=XXXXcyc Trip=XX Scaled=XXXXcyc [header] ===
WARM_PATTERN = re.compile(r"Block \(loop\):.*Warm=(\d+)cyc")

# Pattern to extract warm cycles from note.txt migration target
# Format: "XX% xdl util, YYY warm cycles"
NOTE_WARM_PATTERN = re.compile(r"(\d+)\s*warm cycles")

# Pattern to extract warm cycles from note.txt Sched Perf section
# Format: "Sched Perf\nXXX warm cycles"
SCHED_PERF_PATTERN = re.compile(r"Sched Perf\s*\n\s*(\d+)\s*warm cycles")

# Pattern to match the Flags section header in note.txt
# Supports both "Flags" and "Flags:" formats
FLAGS_SECTION_PATTERN = re.compile(r"^Flags:?\s*$", re.MULTILINE)


def get_llc_path() -> Path:
    """Find llc binary, preferring LLC_PATH if set, else build directory."""
    # Environment variable takes precedence so cross-tree comparisons work.
    if "LLC_PATH" in os.environ:
        return Path(os.environ["LLC_PATH"])

    # Try relative path from repo root
    repo_root = SCRIPT_DIR.parents[4]  # llvm/lib/Target/AMDGPU/PerfCorpus -> repo root
    build_llc = repo_root / "build" / "bin" / "llc"
    if build_llc.exists():
        return build_llc

    # Try PATH
    try:
        result = subprocess.run(["which", "llc"], capture_output=True, text=True)
        if result.returncode == 0:
            return Path(result.stdout.strip())
    except Exception:
        pass

    raise RuntimeError(
        "Cannot find llc. Set LLC_PATH environment variable or ensure build/bin/llc exists."
    )


def find_test_files() -> List[Path]:
    """Find all .ll files in the MI450 corpus."""
    mi450_dir = SCRIPT_DIR / "MI450"
    if not mi450_dir.exists():
        raise RuntimeError(f"MI450 directory not found: {mi450_dir}")
    return sorted(mi450_dir.rglob("*.ll"))


def get_test_name(ll_file: Path) -> str:
    """Generate a unique test name from file path."""
    rel_path = ll_file.relative_to(SCRIPT_DIR / "MI450")
    return str(rel_path.with_suffix(""))


def parse_flags_from_note(note_file: Path) -> List[str]:
    """Parse flags from a note.txt file's Flags section.

    Flags section format:
        Flags:
            --some-flag=value
            --another-flag

    Returns a list of flag strings.
    """
    if not note_file.exists():
        return []

    try:
        content = note_file.read_text()

        # Find the Flags section
        match = FLAGS_SECTION_PATTERN.search(content)
        if not match:
            return []

        # Get content after "Flags:" header
        flags_start = match.end()
        remaining = content[flags_start:]

        flags = []
        for line in remaining.split('\n'):
            # Stop if we hit a non-indented line (next section or empty content)
            stripped = line.strip()
            if not stripped:
                continue
            # If line doesn't start with whitespace, it's a new section
            if line and not line[0].isspace():
                break
            # Parse the flag (should start with --)
            if stripped.startswith('--'):
                flags.append(stripped)

        return flags
    except Exception:
        return []


def run_llc(llc_path: Path, ll_file: Path, config: Config = REGRESSION_CONFIG, sched_only: bool = False, user_flags: Optional[List[str]] = None) -> Tuple[Optional[int], Optional[str]]:
    """Run llc under a configuration and extract the innermost loop warm cycles."""
    cmd = [str(llc_path), str(ll_file), "-o", "-"]
    cmd.extend(config.flags)

    # Add sched-only flag if requested
    if sched_only:
        cmd.append("--amdgpu-static-sim-measure-sched=1")

    # Append the per-test Flags section from note.txt when the config uses it.
    if config.use_note_flags:
        cmd.extend(parse_flags_from_note(ll_file.parent / "note.txt"))

    # Extra flags from --llc-flag, applied to every run.
    if user_flags:
        cmd.extend(user_flags)

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            return None, f"llc failed: {result.stderr[:200]}"

        # Find all warm cycle counts and return the one for the innermost loop
        # The innermost loop is typically the last one encountered before "Inner Loop Header"
        output = result.stdout
        warm_cycles = None

        # Find all matches
        matches = list(WARM_PATTERN.finditer(output))
        if not matches:
            return None, "No warm cycle annotation found in output"

        # Use the first loop block (typically the main hot loop)
        warm_cycles = int(matches[0].group(1))
        return warm_cycles, None

    except subprocess.TimeoutExpired:
        return None, "llc timed out after 120 seconds"
    except Exception as e:
        return None, str(e)


def load_baseline(sched_only: bool = False) -> Dict[str, int]:
    """Load baseline warm cycle counts."""
    baseline_file = SCHED_BASELINE_FILE if sched_only else BASELINE_FILE
    if not baseline_file.exists():
        return {}
    with open(baseline_file) as f:
        return json.load(f)


def load_migration_targets() -> Dict[str, int]:
    """Load migration targets from note.txt files in test directories."""
    targets = {}
    mi450_dir = SCRIPT_DIR / "MI450"
    if not mi450_dir.exists():
        return targets

    for ll_file in mi450_dir.rglob("*.ll"):
        test_name = get_test_name(ll_file)
        note_file = ll_file.parent / "note.txt"
        if note_file.exists():
            try:
                content = note_file.read_text()
                match = NOTE_WARM_PATTERN.search(content)
                if match:
                    targets[test_name] = int(match.group(1))
            except Exception:
                pass
    return targets


def load_sched_perf_targets() -> Dict[str, int]:
    """Load Sched Perf targets from note.txt files in test directories."""
    targets = {}
    mi450_dir = SCRIPT_DIR / "MI450"
    if not mi450_dir.exists():
        return targets

    for ll_file in mi450_dir.rglob("*.ll"):
        test_name = get_test_name(ll_file)
        note_file = ll_file.parent / "note.txt"
        if note_file.exists():
            try:
                content = note_file.read_text()
                match = SCHED_PERF_PATTERN.search(content)
                if match:
                    targets[test_name] = int(match.group(1))
            except Exception:
                pass
    return targets


def save_baseline(baseline: Dict[str, int], sched_only: bool = False) -> None:
    """Save baseline warm cycle counts."""
    baseline_file = SCHED_BASELINE_FILE if sched_only else BASELINE_FILE
    with open(baseline_file, "w") as f:
        json.dump(baseline, f, indent=2, sort_keys=True)
        f.write("\n")


def run_tests(verbose: bool = False, migration_target: bool = False, sched_only: bool = False, user_flags: Optional[List[str]] = None) -> List[TestResult]:
    """Run all tests and return results."""
    llc_path = get_llc_path()
    if migration_target:
        if sched_only:
            baseline = load_sched_perf_targets()
        else:
            baseline = load_migration_targets()
    else:
        baseline = load_baseline(sched_only=sched_only)
    ll_files = find_test_files()

    if not ll_files:
        print("No test files found!")
        return []

    results = []

    for ll_file in ll_files:
        test_name = get_test_name(ll_file)
        if verbose:
            print(f"Running: {test_name}...", end=" ", flush=True)

        start = time.monotonic()
        warm_cycles, error = run_llc(llc_path, ll_file, REGRESSION_CONFIG, sched_only=sched_only, user_flags=user_flags)
        elapsed = time.monotonic() - start
        baseline_cycles = baseline.get(test_name)

        if error:
            passed = False
        elif baseline_cycles is None:
            # No baseline/target - pass (target of 0 or more means any result is acceptable)
            passed = True
        else:
            # Pass if cycles improved or stayed the same
            passed = warm_cycles <= baseline_cycles

        result = TestResult(
            name=test_name,
            warm_cycles=warm_cycles,
            baseline_cycles=baseline_cycles,
            passed=passed,
            error=error,
        )
        results.append(result)

        if verbose:
            timing = f" [{elapsed:.2f}s]"
            if error:
                print(f"ERROR: {error}{timing}")
            elif baseline_cycles is None:
                if migration_target or sched_only:
                    print(f"NO TARGET ({warm_cycles} cycles){timing}")
                else:
                    print(f"NEW ({warm_cycles} cycles){timing}")
            elif passed:
                delta = result.delta
                if delta == 0:
                    print(f"PASS ({warm_cycles} cycles, unchanged){timing}")
                else:
                    print(f"PASS ({warm_cycles} cycles, {delta:+d} = {result.delta_percent:+.2f}%){timing}")
            else:
                print(f"FAIL ({warm_cycles} cycles, baseline {baseline_cycles}, +{result.delta} = +{result.delta_percent:.2f}%){timing}")

    return results


def print_summary(results: List[TestResult], migration_target: bool = False, sched_only: bool = False) -> None:
    """Print test summary."""
    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)
    no_baseline = sum(1 for r in results if r.baseline_cycles is None and r.error is None)
    errors = sum(1 for r in results if r.error)

    print("\n" + "=" * 60)
    print(f"Results: {passed} passed, {failed} failed", end="")
    if no_baseline:
        label = "no target" if (migration_target or sched_only) else "new"
        print(f", {no_baseline} {label}", end="")
    if errors:
        print(f", {errors} errors", end="")
    print()

    if failed > 0:
        label = "target" if (migration_target or sched_only) else "baseline"
        print(f"\nFailed tests (exceeded {label}):")
        for r in results:
            if not r.passed and not r.error:
                print(f"  {r.name}: {r.warm_cycles} > {r.baseline_cycles} (+{r.delta} cycles, +{r.delta_percent:.2f}%)")

    if errors > 0:
        print("\nErrors:")
        for r in results:
            if r.error:
                print(f"  {r.name}: {r.error}")


def update_baseline(results: List[TestResult], sched_only: bool = False) -> None:
    """Update baseline with current results."""
    baseline = {}
    for r in results:
        if r.warm_cycles is not None:
            baseline[r.name] = r.warm_cycles
    save_baseline(baseline, sched_only=sched_only)
    baseline_name = "sched_baseline.json" if sched_only else "baseline.json"
    print(f"Updated {baseline_name} with {len(baseline)} entries.")


def llc_version(llc_path: Path) -> str:
    """Return a one-line description of the llc build for report provenance."""
    try:
        out = subprocess.run([str(llc_path), "--version"], capture_output=True, text=True, timeout=30).stdout
        ver = rev = ""
        for line in out.splitlines():
            s = line.strip()
            if s.startswith("LLVM version"):
                ver = s
            if "revision" in s.lower() or s.lower().startswith("git"):
                rev = s
        return " ".join(p for p in (ver, rev) if p) or "unknown"
    except Exception:
        return "unknown"


def measure_config(llc_path: Path, ll_file: Path, config: Config, user_flags: Optional[List[str]] = None) -> Tuple[Optional[int], Optional[str]]:
    """Measure warm cycles for one file under one config (full warm cycles, not sched-only)."""
    return run_llc(llc_path, ll_file, config, sched_only=False, user_flags=user_flags)


def _pct(new: Optional[int], base: Optional[int]) -> Optional[float]:
    if new is None or base is None or base <= 0:
        return None
    return (new - base) / base * 100.0


def _cell(v: Optional[int]) -> str:
    return str(v) if v is not None else "ERR"


def _pct_cell(p: Optional[float]) -> str:
    if p is None:
        return "-"
    return f"{p:+.1f}%"


def generate_report(verbose: bool = False, user_flags: Optional[List[str]] = None) -> int:
    """Run every corpus file under Off / On+tuned and write PERF_REPORT.md."""
    llc_path = get_llc_path()
    ll_files = find_test_files()
    if not ll_files:
        print("No test files found!")
        return 1

    print(f"Generating perf report...")
    print(f"LLC: {llc_path}")
    print()

    # rows: source -> list of (kernel, {config_key: cycles})
    from collections import OrderedDict
    rows_by_source: "OrderedDict[str, list]" = OrderedDict()
    tuned_ratios = []  # On+tuned/Off ratio, <1 = faster

    for ll_file in ll_files:
        test_name = get_test_name(ll_file)
        source = test_name.split(os.sep)[0]
        kernel = os.sep.join(test_name.split(os.sep)[1:])
        cycles = {}
        for cfg in REPORT_CONFIGS:
            if verbose:
                print(f"  {test_name} [{cfg.label}]...", end=" ", flush=True)
            c, err = measure_config(llc_path, ll_file, cfg, user_flags=user_flags)
            cycles[cfg.key] = c
            if verbose:
                print(f"{c if c is not None else 'ERR: ' + (err or '')}")
        rows_by_source.setdefault(source, []).append((kernel, cycles))
        off = cycles.get("off")
        if off and off > 0 and cycles.get("tuned"):
            tuned_ratios.append(cycles["tuned"] / off)

    def geomean_pct(ratios):
        # Geometric mean of the ratios, expressed as percent change vs Off.
        if not ratios:
            return None
        g = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
        return (g - 1.0) * 100.0

    def median_pct(ratios):
        if not ratios:
            return None
        s = sorted(ratios)
        n = len(s)
        m = s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2
        return (m - 1.0) * 100.0

    lines = []
    lines.append("# AMDGPU CoExec Scheduler Perf Report")
    lines.append("")
    lines.append("Warm cycle counts of the innermost hot loop for each corpus kernel, "
                 "measured by the AMDGPU static simulator, comparing the CoExec scheduler "
                 "off versus on (with per-kernel tuning).")
    lines.append("")
    lines.append(f"- llc: `{llc_version(llc_path)}`")
    lines.append(f"- Generated by: `perf_test.py --report`")
    lines.append("")
    lines.append("Lower is better. The `Δ` column is relative to **Off** (negative = faster).")
    lines.append("")
    lines.append("## Configurations")
    lines.append("")
    lines.append(f"- **Base** (shared by all): `{' '.join(BASE_FLAGS)}`")
    lines.append(f"- **Off**: base only, default scheduler.")
    lines.append(f"- **On+tuned**: base + `{' '.join(COEXEC_FLAGS)}` + the per-test flags "
                 f"from each kernel's `note.txt` Flags section.")
    lines.append("")

    for source, rows in rows_by_source.items():
        lines.append(f"## {source}")
        lines.append("")
        lines.append("| Kernel | Off | On+tuned | Δ |")
        lines.append("|---|---:|---:|---:|")
        for kernel, cyc in rows:
            lines.append(
                f"| {kernel} | {_cell(cyc.get('off'))} | {_cell(cyc.get('tuned'))} | "
                f"{_pct_cell(_pct(cyc.get('tuned'), cyc.get('off')))} |"
            )
        lines.append("")

    lines.append("## Summary")
    lines.append("")
    total = sum(len(r) for r in rows_by_source.values())
    improved = sum(1 for r in tuned_ratios if r < 1.0)
    regressed = sum(1 for r in tuned_ratios if r > 1.0)
    unchanged = sum(1 for r in tuned_ratios if r == 1.0)
    g_tuned = geomean_pct(tuned_ratios)
    m_tuned = median_pct(tuned_ratios)
    lines.append(f"- Kernels measured: {total}")
    lines.append(f"- On+tuned vs Off: {improved} faster, {regressed} slower, {unchanged} unchanged")
    if g_tuned is not None:
        lines.append(f"- Geomean Δ On+tuned vs Off: {g_tuned:+.1f}%")
    if m_tuned is not None:
        lines.append(f"- Median Δ On+tuned vs Off: {m_tuned:+.1f}%")
    lines.append("")
    lines.append("Geometric mean is used to average per-kernel ratios; it is not skewed "
                 "by a single large outlier the way an arithmetic mean of percentages is.")
    lines.append("")

    REPORT_FILE.write_text("\n".join(lines) + "\n")
    print(f"\nWrote {REPORT_FILE}")
    if g_tuned is not None:
        print(f"  {total} kernels, geomean On+tuned vs Off {g_tuned:+.1f}%")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Run performance regression tests")
    parser.add_argument("--update", action="store_true",
                        help="Update baseline with current results")
    parser.add_argument("--update-on-pass", action="store_true",
                        help="Update baseline only if all tests pass")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show detailed output for each test")
    parser.add_argument("--list", action="store_true",
                        help="List all test files without running")
    parser.add_argument("--migration-target", action="store_true",
                        help="Use warm cycles from note.txt files as baseline instead of baseline.json")
    parser.add_argument("--sched-only", action="store_true",
                        help="Use Sched Perf entries from note.txt and add --amdgpu-static-sim-measure-sched=1 to llc")
    parser.add_argument("--llc-flag", action="append", default=[], metavar="FLAG",
                        dest="llc_flags",
                        help="Extra llc flag applied to every kernel run (repeatable).")
    parser.add_argument("--report", action="store_true",
                        help="Run Off/On+tuned for every kernel and write PERF_REPORT.md")
    args = parser.parse_args()

    if args.list:
        for f in find_test_files():
            print(get_test_name(f))
        return 0

    if args.report:
        return generate_report(verbose=args.verbose, user_flags=args.llc_flags)

    print(f"Running performance tests...")
    print(f"LLC: {get_llc_path()}")
    if args.llc_flags:
        print(f"Extra llc flags: {' '.join(args.llc_flags)}")

    # --update updates both baselines, ignoring --sched-only
    if args.update or args.update_on_pass:
        print(f"Baseline: {BASELINE_FILE}")
        print()
        results = run_tests(verbose=args.verbose, migration_target=args.migration_target, sched_only=False, user_flags=args.llc_flags)
        print_summary(results, migration_target=args.migration_target, sched_only=False)
        all_passed = all(r.passed for r in results)

        print(f"\nRunning sched-only tests...")
        print(f"Baseline: {SCHED_BASELINE_FILE}")
        print(f"Mode: sched-only (--amdgpu-static-sim-measure-sched=1)")
        print()
        sched_results = run_tests(verbose=args.verbose, migration_target=args.migration_target, sched_only=True, user_flags=args.llc_flags)
        print_summary(sched_results, migration_target=args.migration_target, sched_only=True)
        sched_all_passed = all(r.passed for r in sched_results)

        if args.update:
            update_baseline(results, sched_only=False)
            update_baseline(sched_results, sched_only=True)
        elif args.update_on_pass and all_passed and sched_all_passed:
            update_baseline(results, sched_only=False)
            update_baseline(sched_results, sched_only=True)

        return 0 if (all_passed and sched_all_passed) else 1

    # Normal run (not updating)
    if args.sched_only:
        if args.migration_target:
            print(f"Baseline: Sched Perf targets from note.txt files")
        else:
            print(f"Baseline: {SCHED_BASELINE_FILE}")
        print(f"Mode: sched-only (--amdgpu-static-sim-measure-sched=1)")
    elif args.migration_target:
        print(f"Baseline: migration targets from note.txt files")
    else:
        print(f"Baseline: {BASELINE_FILE}")
    print()

    results = run_tests(verbose=args.verbose, migration_target=args.migration_target, sched_only=args.sched_only, user_flags=args.llc_flags)
    print_summary(results, migration_target=args.migration_target, sched_only=args.sched_only)

    all_passed = all(r.passed for r in results)

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
