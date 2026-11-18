#!/usr/bin/env python3
"""
CMA-ES optimizer for AMDGPU CoExec scheduler weights.

Implements the (μ/μ_W, λ)-CMA-ES from:
  Hansen, N. "The CMA Evolution Strategy: A Tutorial" (arXiv:1604.00772v2)
  Appendix A, Figure 6, equations (38)-(58).

Objective: minimize sum of Stall Percent across all kernels in the corpus,
as reported by the AMDGPU static simulator's Scaled Metrics section.

The 21 scoring weights are searched in log10 space (x_i = log10(weight_i)),
which naturally handles the many-orders-of-magnitude default scale and
enforces the expected non-negativity.

Usage:
    python3 cmaes_optimize.py                      # Run with defaults
    python3 cmaes_optimize.py --max-generations 50  # Limit generations
    python3 cmaes_optimize.py --sigma0 1.5          # Initial step-size in log10
    python3 cmaes_optimize.py --workers 16          # Parallel workers
    python3 cmaes_optimize.py --resume checkpoint.json  # Resume from checkpoint
    python3 cmaes_optimize.py --kernel-dir /path/to/kernels  # Custom kernel dir
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
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import List, Optional, Tuple, Dict

import numpy as np

# ---------------------------------------------------------------------------
# Weight definitions
# ---------------------------------------------------------------------------

WEIGHT_NAMES = [
    "WBiasPhysReg",
    "WRegExcess",
    "WStall",
    "WMissedSlots",
    "WMemoryPipeline",
    "WCoexecSlotAvoid",
    "WCoexecSlotPrefer",
    "WShadowMixDemandSatisfied",
    "WShadowMixDirectEnable",
    "WShadowMixLookahead",
    "WShadowMixGreedy",
    "WCritResource",
    "WCritResourceWMMAPrio",
    "WCritResourceDep",
    "WCritResourceDepLoopCarried",
    "WCritResourcePrio",
    "WCluster",
    "WWeak",
    "WRegMax",
    "WLatency",
    "WNodeOrder",
]

DEFAULT_WEIGHTS = [
    1e15, 1e14, 1e13, 1e12, 1e11, 1e10, 1e9,
    1e8, 5e7, 2.5e7, 1.25e7,
    1e6, 5e5, 1e5, 5e4, 1e4,
    1e3, 1e2, 1e1, 1e0, 1e-1,
]

N_WEIGHTS = len(WEIGHT_NAMES)

# ---------------------------------------------------------------------------
# Search-space encoding: log10
# ---------------------------------------------------------------------------

def weights_to_log(weights: List[float]) -> np.ndarray:
    return np.array([math.log10(max(w, 1e-30)) for w in weights])


def log_to_weights(x: np.ndarray) -> List[float]:
    return [10.0 ** float(xi) for xi in x]

# ---------------------------------------------------------------------------
# Fitness evaluation
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).parent.resolve()

DEFAULT_LLC = SCRIPT_DIR.parents[4] / "build" / "bin" / "llc"
DEFAULT_KERNEL_DIR = Path("/home/jeff/source/MLKernelCorpus/MI450/kernels")

BASE_FLAGS = [
    "-march=amdgcn",
    "-mcpu=gfx1250",
    "-amdgpu-enable-static-simulator",
    "-amdgpu-static-sim-inline",
    "-amdgpu-expert-scheduling-mode",
    "-amdgpu-anti-hints-for-va-vdst",
    "--amdgpu-coexec-use-scoring=1",
]

STALL_PATTERN = re.compile(
    r"=== Scaled Metrics.*?Stall:\s*\d+\s*cycles\s*\((\d+(?:\.\d+)?)%\)",
    re.DOTALL,
)


def find_kernels(kernel_dir: Path) -> List[Path]:
    exts = {".ll", ".llir"}
    kernels = sorted(
        p for p in kernel_dir.rglob("*") if p.suffix in exts and p.is_file()
    )
    return kernels


def write_config_json(weights: List[float], path: Path) -> None:
    config: Dict = {"UseScoring": True}
    for name, w in zip(WEIGHT_NAMES, weights):
        config[name] = w
    path.write_text(json.dumps(config, indent=2) + "\n")


def _evaluate_single_kernel(
    llc_path: str, kernel_path: str, config_path: str
) -> Tuple[str, Optional[float], Optional[str]]:
    """Worker function: compile one kernel, return (kernel_name, stall%, error)."""
    cmd = [llc_path, kernel_path, "-o", "-"]
    cmd.extend(BASE_FLAGS)
    cmd.append(f"--amdgpu-sched-config={config_path}")

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=180
        )
        if result.returncode != 0:
            return (kernel_path, None, f"llc exit {result.returncode}")

        m = STALL_PATTERN.search(result.stdout)
        if m:
            return (kernel_path, float(m.group(1)), None)

        return (kernel_path, None, "no Scaled Metrics / Stall line")

    except subprocess.TimeoutExpired:
        return (kernel_path, None, "timeout")
    except Exception as e:
        return (kernel_path, None, str(e))


def evaluate_candidate(
    weights: List[float],
    kernels: List[Path],
    llc_path: Path,
    config_path: Path,
    workers: int,
) -> Tuple[float, int, int]:
    """Evaluate a candidate weight vector.

    Returns (sum_stall_pct, n_scored, n_failed).
    """
    write_config_json(weights, config_path)
    llc_str = str(llc_path)
    cfg_str = str(config_path)

    total_stall = 0.0
    n_scored = 0
    n_failed = 0

    with ProcessPoolExecutor(max_workers=workers) as pool:
        futures = {
            pool.submit(
                _evaluate_single_kernel, llc_str, str(k), cfg_str
            ): k
            for k in kernels
        }
        for fut in as_completed(futures):
            _kpath, stall, err = fut.result()
            if stall is not None:
                total_stall += stall
                n_scored += 1
            else:
                n_failed += 1

    total_stall += n_failed * 100.0
    return total_stall, n_scored, n_failed


def evaluate_generation(
    candidate_weights: List[List[float]],
    kernels: List[Path],
    llc_path: Path,
    work_dir: Path,
    gen: int,
    workers: int,
) -> List[Tuple[float, int, int]]:
    """Evaluate ALL candidates in one parallel batch.

    Submits every (candidate × kernel) job to a single process pool so CPU
    cores stay saturated across candidates instead of processing them one
    at a time.  With 13 candidates × 397 kernels = 5161 jobs, this cuts
    generation time from ~13× single-candidate time down to ~1×.
    """
    n_cands = len(candidate_weights)
    llc_str = str(llc_path)

    config_paths: List[Path] = []
    for ci, weights in enumerate(candidate_weights):
        cfg = work_dir / f"config_gen{gen}_c{ci}.json"
        write_config_json(weights, cfg)
        config_paths.append(cfg)

    results_by_cand: List[List[Optional[float]]] = [[] for _ in range(n_cands)]

    with ProcessPoolExecutor(max_workers=workers) as pool:
        future_to_ci = {}
        for ci in range(n_cands):
            cfg_str = str(config_paths[ci])
            for k in kernels:
                fut = pool.submit(
                    _evaluate_single_kernel, llc_str, str(k), cfg_str
                )
                future_to_ci[fut] = ci

        for fut in as_completed(future_to_ci):
            ci = future_to_ci[fut]
            _kpath, stall, _err = fut.result()
            results_by_cand[ci].append(stall)

    out: List[Tuple[float, int, int]] = []
    for ci in range(n_cands):
        total = 0.0
        scored = 0
        failed = 0
        for s in results_by_cand[ci]:
            if s is not None:
                total += s
                scored += 1
            else:
                failed += 1
                total += 100.0
        out.append((total, scored, failed))

    for cfg in config_paths:
        cfg.unlink(missing_ok=True)

    return out


# ---------------------------------------------------------------------------
# CMA-ES  (μ/μ_W, λ)-CMA-ES
# Reference: Hansen tutorial, Appendix A (Figure 6) + Table 1 (eqs 48-58)
# ---------------------------------------------------------------------------

class CMAES:
    """Full (μ/μ_W, λ)-CMA-ES with active covariance matrix adaptation."""

    def __init__(self, x0: np.ndarray, sigma0: float):
        n = len(x0)
        self.n = n

        # --- Strategy parameters (Table 1, eqs 48-58) ---
        # Population size (eq 48)
        self.lam = 4 + int(3 * math.log(n))
        # Number of parents (eq 48 note: mu = floor(lambda/2))
        self.mu = self.lam // 2

        # Recombination weights (eqs 49, 53)
        raw_w = np.array(
            [math.log((self.lam + 1) / 2) - math.log(i + 1) for i in range(self.lam)]
        )
        # Positive weights sum to 1
        pos = raw_w[: self.mu]
        self.weights_pos = pos / pos.sum()
        # Variance effective selection mass (eq 8)
        self.mu_eff = 1.0 / (self.weights_pos ** 2).sum()

        # Negative weights (eq 53)
        neg = raw_w[self.mu :]
        mu_eff_neg = 1.0 / ((neg ** 2).sum() / (neg.sum() ** 2)) if neg.sum() != 0 else 0

        # Learning rates (eqs 55-58)
        self.c_sigma = (self.mu_eff + 2) / (n + self.mu_eff + 5)  # eq 55
        self.d_sigma = (
            1
            + 2 * max(0, math.sqrt((self.mu_eff - 1) / (n + 1)) - 1)
            + self.c_sigma
        )  # eq 55
        self.c_c = (4 + self.mu_eff / n) / (n + 4 + 2 * self.mu_eff / n)  # eq 56
        alpha_cov = 2.0
        self.c_1 = alpha_cov / ((n + 1.3) ** 2 + self.mu_eff)  # eq 57
        self.c_mu = min(
            1 - self.c_1,
            alpha_cov
            * (1 / 4 + self.mu_eff + 1 / self.mu_eff - 2)
            / ((n + 2) ** 2 + alpha_cov * self.mu_eff / 2),
        )  # eq 58

        # Full weight vector for covariance update (eq 53)
        alpha_mu_neg = 1 + self.c_1 / self.c_mu  # eq 50
        alpha_mu_eff_neg = (
            1 + 2 * mu_eff_neg / (self.mu_eff + 2)
        )  # eq 51
        alpha_pos_def = (1 - self.c_1 - self.c_mu) / (n * self.c_mu)  # eq 52

        self.weights_all = np.zeros(self.lam)
        self.weights_all[: self.mu] = self.weights_pos
        if neg.sum() != 0:
            neg_scale = min(alpha_mu_neg, alpha_mu_eff_neg, alpha_pos_def)
            self.weights_all[self.mu :] = neg_scale * neg / abs(neg.sum())
        self.sum_w = self.weights_all.sum()

        self.c_m = 1.0  # eq 54

        # --- Dynamic state ---
        self.mean = x0.copy()
        self.sigma = sigma0
        self.C = np.eye(n)
        self.p_sigma = np.zeros(n)
        self.p_c = np.zeros(n)
        self.B = np.eye(n)
        self.D = np.ones(n)
        self.gen = 0
        self.eigeneval = 0

        # E‖N(0,I)‖ ≈ √n (1 - 1/(4n) + 1/(21n²))
        self.chi_n = math.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n ** 2))

        # Best-ever tracking
        self.best_fitness = float("inf")
        self.best_x = x0.copy()

    def ask(self) -> List[np.ndarray]:
        """Sample λ candidate solutions."""
        candidates = []
        for _ in range(self.lam):
            z = np.random.randn(self.n)
            y = self.B @ (self.D * z)  # eq 39: y ~ N(0, C)
            x = self.mean + self.sigma * y  # eq 40
            candidates.append(x)
        return candidates

    def tell(self, candidates: List[np.ndarray], fitnesses: List[float]) -> None:
        """Update CMA-ES state given evaluated candidates and their fitness."""
        n = self.n
        lam = self.lam

        # Sort by fitness (ascending = minimization)
        idx = np.argsort(fitnesses)
        sorted_candidates = [candidates[i] for i in idx]
        sorted_fitnesses = [fitnesses[i] for i in idx]

        # Track best-ever
        if sorted_fitnesses[0] < self.best_fitness:
            self.best_fitness = sorted_fitnesses[0]
            self.best_x = sorted_candidates[0].copy()

        # Compute y_{i:λ} = (x_{i:λ} - m_old) / σ
        x_old = self.mean.copy()
        y_all = [(x - x_old) / self.sigma for x in sorted_candidates]

        # Selection and recombination: ⟨y⟩_w = Σ w_i * y_{i:λ} (eq 41)
        y_w = sum(self.weights_pos[i] * y_all[i] for i in range(self.mu))

        # Update mean (eq 42)
        self.mean = x_old + self.c_m * self.sigma * y_w

        # C^{-1/2} = B * D^{-1} * B^T
        invsqrtC = self.B @ np.diag(1.0 / self.D) @ self.B.T

        # Step-size control: update p_σ (eq 43)
        self.p_sigma = (1 - self.c_sigma) * self.p_sigma + math.sqrt(
            self.c_sigma * (2 - self.c_sigma) * self.mu_eff
        ) * (invsqrtC @ y_w)

        # Update σ (eq 44)
        self.sigma *= math.exp(
            (self.c_sigma / self.d_sigma)
            * (np.linalg.norm(self.p_sigma) / self.chi_n - 1)
        )

        # Heaviside function h_σ (Appendix A)
        h_sigma_threshold = (1.4 + 2.0 / (n + 1)) * self.chi_n * math.sqrt(
            1 - (1 - self.c_sigma) ** (2 * (self.gen + 1))
        )
        h_sigma = 1.0 if np.linalg.norm(self.p_sigma) < h_sigma_threshold else 0.0

        # Update p_c (eq 45)
        self.p_c = (1 - self.c_c) * self.p_c + h_sigma * math.sqrt(
            self.c_c * (2 - self.c_c) * self.mu_eff
        ) * y_w

        # δ(h_σ) correction
        delta_h = (1 - h_sigma) * self.c_c * (2 - self.c_c)

        # Covariance matrix adaptation (eq 47)
        # w_i° for the covariance update (eq 46)
        w_circ = np.zeros(lam)
        for i in range(lam):
            if self.weights_all[i] >= 0:
                w_circ[i] = self.weights_all[i]
            else:
                # Scale by n / ‖C^{-1/2} * y_{i:λ}‖²
                z_i = invsqrtC @ y_all[idx[i]]
                w_circ[i] = self.weights_all[i] * n / (np.linalg.norm(z_i) ** 2)

        rank_one = np.outer(self.p_c, self.p_c)
        rank_mu = sum(
            w_circ[i] * np.outer(y_all[idx[i]], y_all[idx[i]])
            for i in range(lam)
        )

        self.C = (
            (1 + self.c_1 * delta_h - self.c_1 - self.c_mu * self.sum_w) * self.C
            + self.c_1 * rank_one
            + self.c_mu * rank_mu
        )

        # Enforce symmetry
        self.C = np.triu(self.C) + np.triu(self.C, 1).T

        # Update B, D from eigendecomposition (periodic, for efficiency)
        self.eigeneval += lam
        if self.eigeneval >= lam / (self.c_1 + self.c_mu) / n / 10:
            self.eigeneval = 0
            eigenvalues, self.B = np.linalg.eigh(self.C)
            eigenvalues = np.maximum(eigenvalues, 1e-20)
            self.D = np.sqrt(eigenvalues)

        self.gen += 1

    def should_stop(self) -> Optional[str]:
        """Check termination criteria (Section B.3)."""
        # ConditionCov: condition number > 1e14
        if self.D.max() / self.D.min() > 1e7:
            return "ConditionCov"
        # TolXUp: σ * max(D) grew by more than 1e4
        if self.sigma * self.D.max() > 1e4 * self.sigma:
            pass  # Only trigger on initial comparison, skip for now
        # NoEffectCoord: adding 0.2*σ*c_ii doesn't change mean
        diag_C = np.diag(self.C)
        if np.all(self.mean == self.mean + 0.2 * self.sigma * np.sqrt(diag_C)):
            return "NoEffectCoord"
        return None

    def state_dict(self) -> Dict:
        """Serialize full state for checkpointing."""
        return {
            "gen": self.gen,
            "n": self.n,
            "lam": self.lam,
            "mu": self.mu,
            "mean": self.mean.tolist(),
            "sigma": self.sigma,
            "C": self.C.tolist(),
            "p_sigma": self.p_sigma.tolist(),
            "p_c": self.p_c.tolist(),
            "B": self.B.tolist(),
            "D": self.D.tolist(),
            "eigeneval": self.eigeneval,
            "best_fitness": self.best_fitness,
            "best_x": self.best_x.tolist(),
            "weights_pos": self.weights_pos.tolist(),
            "weights_all": self.weights_all.tolist(),
            "mu_eff": self.mu_eff,
            "c_sigma": self.c_sigma,
            "d_sigma": self.d_sigma,
            "c_c": self.c_c,
            "c_1": self.c_1,
            "c_mu": self.c_mu,
            "c_m": self.c_m,
            "sum_w": self.sum_w,
            "chi_n": self.chi_n,
        }

    @classmethod
    def from_state_dict(cls, d: Dict) -> "CMAES":
        """Restore from checkpoint."""
        x0 = np.array(d["mean"])
        obj = cls.__new__(cls)
        obj.n = d["n"]
        obj.lam = d["lam"]
        obj.mu = d["mu"]
        obj.mean = x0
        obj.sigma = d["sigma"]
        obj.C = np.array(d["C"])
        obj.p_sigma = np.array(d["p_sigma"])
        obj.p_c = np.array(d["p_c"])
        obj.B = np.array(d["B"])
        obj.D = np.array(d["D"])
        obj.gen = d["gen"]
        obj.eigeneval = d["eigeneval"]
        obj.best_fitness = d["best_fitness"]
        obj.best_x = np.array(d["best_x"])
        obj.weights_pos = np.array(d["weights_pos"])
        obj.weights_all = np.array(d["weights_all"])
        obj.mu_eff = d["mu_eff"]
        obj.c_sigma = d["c_sigma"]
        obj.d_sigma = d["d_sigma"]
        obj.c_c = d["c_c"]
        obj.c_1 = d["c_1"]
        obj.c_mu = d["c_mu"]
        obj.c_m = d["c_m"]
        obj.sum_w = d["sum_w"]
        obj.chi_n = d["chi_n"]
        return obj

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

def log_generation(
    gen: int,
    fitnesses: List[float],
    best_ever: float,
    sigma: float,
    elapsed: float,
    mean_weights: List[float],
):
    best_gen = min(fitnesses)
    median_gen = sorted(fitnesses)[len(fitnesses) // 2]
    worst_gen = max(fitnesses)
    print(
        f"gen {gen:4d} | best_gen {best_gen:8.2f} | median {median_gen:8.2f} | "
        f"worst {worst_gen:8.2f} | best_ever {best_ever:8.2f} | "
        f"σ {sigma:.4f} | {elapsed:.1f}s"
    )
    sys.stdout.flush()


def write_best_config(weights: List[float], path: Path) -> None:
    config: Dict = {"UseScoring": True}
    for name, w in zip(WEIGHT_NAMES, weights):
        config[name] = w
    path.write_text(json.dumps(config, indent=2) + "\n")
    print(f"  -> Best config written to {path}")


def write_generation_log(log_path: Path, entry: Dict) -> None:
    with open(log_path, "a") as f:
        f.write(json.dumps(entry) + "\n")

# ---------------------------------------------------------------------------
# Main optimization loop
# ---------------------------------------------------------------------------

def run_optimization(args):
    llc_path = Path(args.llc).resolve()
    kernel_dir = Path(args.kernel_dir).resolve()
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    kernels = find_kernels(kernel_dir)
    if not kernels:
        print(f"No kernels found in {kernel_dir}")
        return 1
    print(f"Found {len(kernels)} kernels in {kernel_dir}")
    print(f"LLC: {llc_path}")
    print(f"Output dir: {out_dir}")
    print(f"Workers: {args.workers}")
    print()

    # Initialize or resume CMA-ES
    if args.resume:
        print(f"Resuming from {args.resume}")
        with open(args.resume) as f:
            state = json.load(f)
        es = CMAES.from_state_dict(state)
        print(f"  Resumed at generation {es.gen}, best_ever = {es.best_fitness:.2f}")
    else:
        x0 = weights_to_log(DEFAULT_WEIGHTS)
        es = CMAES(x0, args.sigma0)
        print(f"CMA-ES initialized: n={es.n}, λ={es.lam}, μ={es.mu}")
        print(f"  σ₀={args.sigma0} (in log10 space)")
        print(f"  μ_eff={es.mu_eff:.2f}")
        print(f"  c_σ={es.c_sigma:.4f}, d_σ={es.d_sigma:.4f}")
        print(f"  c_c={es.c_c:.4f}, c_1={es.c_1:.4f}, c_μ={es.c_mu:.4f}")

    # Evaluate the default weights first (generation 0 baseline)
    if not args.resume:
        print("\nEvaluating default weights as baseline...")
        config_path = out_dir / "config_baseline.json"
        t0 = time.monotonic()
        baseline_fitness, n_scored, n_failed = evaluate_candidate(
            DEFAULT_WEIGHTS, kernels, llc_path, config_path, args.workers
        )
        elapsed = time.monotonic() - t0
        print(
            f"  Baseline: sum(stall%) = {baseline_fitness:.2f} "
            f"({n_scored} scored, {n_failed} failed) [{elapsed:.1f}s]"
        )
        es.best_fitness = baseline_fitness
        es.best_x = weights_to_log(DEFAULT_WEIGHTS)
        print()

    log_path = out_dir / "generations.jsonl"
    checkpoint_path = out_dir / "checkpoint.json"
    best_config_path = out_dir / "best_config.json"

    print(f"Starting CMA-ES optimization (max {args.max_generations} generations)")
    print(f"{'='*80}")

    for gen_i in range(args.max_generations):
        gen_start = time.monotonic()

        # Sample candidates in log10 space
        candidates_log = es.ask()

        # Evaluate ALL candidates in one parallel batch
        all_weights = [log_to_weights(x) for x in candidates_log]
        gen_results = evaluate_generation(
            all_weights, kernels, llc_path, out_dir, es.gen, args.workers
        )
        fitnesses = [r[0] for r in gen_results]

        gen_elapsed = time.monotonic() - gen_start

        # Update CMA-ES
        es.tell(candidates_log, fitnesses)

        # Log
        mean_weights = log_to_weights(es.mean)
        log_generation(
            es.gen, fitnesses, es.best_fitness, es.sigma, gen_elapsed, mean_weights
        )

        gen_entry = {
            "gen": es.gen,
            "best_gen": min(fitnesses),
            "median": sorted(fitnesses)[len(fitnesses) // 2],
            "worst": max(fitnesses),
            "best_ever": es.best_fitness,
            "sigma": es.sigma,
            "mean_log": es.mean.tolist(),
            "best_x_log": es.best_x.tolist(),
            "fitnesses": fitnesses,
            "elapsed_s": gen_elapsed,
        }
        write_generation_log(log_path, gen_entry)

        # Write best config and checkpoint every generation
        best_weights = log_to_weights(es.best_x)
        write_best_config(best_weights, best_config_path)

        with open(checkpoint_path, "w") as f:
            json.dump(es.state_dict(), f)

        # Termination criteria
        stop_reason = es.should_stop()
        if stop_reason:
            print(f"\nStopping: {stop_reason}")
            break

        # TolFun: fitness range of last 10+⌈30n/λ⌉ generations is below threshold
        # (Simplified: check if fitness hasn't improved in many generations)

    print(f"\n{'='*80}")
    print(f"Optimization complete after {es.gen} generations.")
    print(f"Best fitness (sum stall%): {es.best_fitness:.2f}")
    print(f"\nBest weights:")
    best_weights = log_to_weights(es.best_x)
    for name, w in zip(WEIGHT_NAMES, best_weights):
        default = DEFAULT_WEIGHTS[WEIGHT_NAMES.index(name)]
        ratio = w / default if default != 0 else float("inf")
        print(f"  {name:35s} = {w:12.4e}  (×{ratio:.3f} vs default)")
    print(f"\nBest config: {best_config_path}")
    print(f"Checkpoint:  {checkpoint_path}")
    print(f"Log:         {log_path}")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="CMA-ES optimizer for CoExec scheduler weights"
    )
    parser.add_argument(
        "--llc",
        default=str(DEFAULT_LLC),
        help="Path to llc binary",
    )
    parser.add_argument(
        "--kernel-dir",
        default=str(DEFAULT_KERNEL_DIR),
        help="Directory containing .ll/.llir kernel files",
    )
    parser.add_argument(
        "--out-dir",
        default=str(SCRIPT_DIR / "cmaes_results"),
        help="Output directory for logs, checkpoints, configs",
    )
    parser.add_argument(
        "--sigma0",
        type=float,
        default=1.0,
        help="Initial step-size in log10 space (default: 1.0 = ±1 order of magnitude)",
    )
    parser.add_argument(
        "--max-generations",
        type=int,
        default=200,
        help="Maximum number of CMA-ES generations",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=max(1, len(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else os.cpu_count() or 1),
        help="Number of parallel workers for kernel evaluation",
    )
    parser.add_argument(
        "--resume",
        type=str,
        default=None,
        help="Path to checkpoint.json to resume from",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Random seed for reproducibility",
    )
    args = parser.parse_args()

    if args.seed is not None:
        np.random.seed(args.seed)

    return run_optimization(args)


if __name__ == "__main__":
    sys.exit(main())
