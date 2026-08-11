# AMDGPU Performance Corpus

This directory contains IR test files and tooling for tracking performance of the AMDGPU static simulator's warm cycle counts.

## Quick Start

```bash
# Run all tests against baseline
./perf_test.py

# Run with detailed output
./perf_test.py --verbose
```

## Directory Structure

```
PerfCorpus/
├── perf_test.py      # Test runner script
├── baseline.json     # Tracked warm cycle counts
├── PERF_REPORT.md    # Generated: CoExec off vs on, per kernel
├── README.md
└── MI450/            # Test corpus organized by source
    ├── AAI/
    │   ├── bf16FA/
    │   ├── bf16_GEMM/
    │   └── ...
    └── Gluon/
        ├── bf16FA/
        ├── fp8_GEMM/
        └── ...
```

## Workflows

### Checking for Regressions

Before committing compiler changes, run the tests to ensure no regressions:

```bash
./perf_test.py --verbose
```

- **PASS**: Warm cycles stayed the same or improved
- **FAIL**: Warm cycles increased (regression)
- **NEW**: No baseline exists for this test

Exit code is 0 if all tests pass, 1 if any fail.

### Updating the Baseline

After making intentional performance changes, update the baseline:

```bash
# Update unconditionally
./perf_test.py --update

# Update only if all tests pass (safer)
./perf_test.py --update-on-pass
```

Commit the updated `baseline.json` along with your compiler changes.

### Adding New Tests

1. Add `.ll` files under `MI450/<source>/<kernel>/`
2. Run `./perf_test.py --verbose` to see the new test detected as "NEW"
3. Run `./perf_test.py --update` to add it to the baseline

### Listing Tests

```bash
./perf_test.py --list
```

### Testing Against Migration Targets

Some test directories contain a `note.txt` file with migration target warm cycles (e.g., "722 warm cycles"). Use `--migration-target` to test against these targets instead of `baseline.json`:

```bash
./perf_test.py --migration-target --verbose
```

Tests without a corresponding `note.txt` automatically pass (any cycle count is acceptable).

## Perf Report

`PERF_REPORT.md` is a generated page summarizing performance with and without the
CoExec scheduler across the whole corpus:

```bash
./perf_test.py --report
```

It measures Off versus On+tuned per kernel and writes a table with per-kernel deltas
and a geomean summary. Regenerate and commit it after compiler changes that move the
numbers.

## Configurations

All configurations share a **base** flag set, which is the measurement environment
rather than scheduler tuning:

```
-mtriple=amdgcn-amd-amdhsa -mcpu=gfx1250 \
    --amdgpu-enable-static-simulator=1 \   # emits the Warm= cycle annotations
    --fp-contract=fast \                   # affects the IR lowering all configs share
    --amdgpu-expert-scheduling-mode \
    --enable-post-misched=0                # disable post-RA scheduling
```

| Config     | Flags on top of base                                             |
|------------|-----------------------------------------------------------------|
| Off        | (none) - default scheduler                                      |
| On+tuned   | `--amdgpu-sched-strategy=coexec --amdgpu-anti-hints-for-va-vdst` + the per-test flags from each kernel's `note.txt` Flags section |

The report compares **Off** versus **On+tuned**.

The regression gate (plain `./perf_test.py`) and `baseline.json` measure the
**On+tuned** configuration.

## How It Works

The script runs each `.ll` file through `llc` under the configuration above and
extracts the `Warm=` cycle count from the innermost loop block annotation, comparing
against the stored baseline.

## Configuration

The script finds `llc` automatically from `../../../../../../build/bin/llc` (relative to repo root). Override with:

```bash
LLC_PATH=/path/to/llc ./perf_test.py
```
