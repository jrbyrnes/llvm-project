//===-- MLCoExecSchedStrategy.h - CoExec ML Scheduler Strategy -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Machine Learning guided co-execution scheduling strategy for AMDGPU.
/// This class extends AMDGPUCoExecSchedStrategy with MLGO (Machine Learning
/// Guided Optimization) infrastructure for training and inference.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_MLCOEXECSCHEDSTRATEGY_H
#define LLVM_LIB_TARGET_AMDGPU_MLCOEXECSCHEDSTRATEGY_H

#include "AMDGPUCoExecSchedStrategy.h"
#include "llvm/Analysis/MLModelRunner.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Support/JSON.h"
#include <cstdint>
#include <memory>

namespace llvm {

// Forward declaration for Logger (defined in TrainingLogger.h)
class Logger;

// Forward declaration for TCP client (defined in .cpp)
class InferenceServerClient;

/// MLCoExecSchedStrategy extends AMDGPUCoExecSchedStrategy with MLGO
/// infrastructure for machine learning guided instruction scheduling.
///
/// This scheduler preserves all CoExec-specific behavior (WMMA co-execution
/// window management, hardware unit tracking, etc.) while adding:
/// - ML model runner for inference (Development/Release modes)
/// - Logger for training data collection
/// - Feature extraction for ML model input
///
/// Currently implements pure fallback mode - all scheduling decisions defer
/// to the parent AMDGPUCoExecSchedStrategy. ML-guided node selection will
/// be added in future iterations.
class MLCoExecSchedStrategy final : public AMDGPUCoExecSchedStrategy {
public:
  MLCoExecSchedStrategy(const MachineSchedContext *C);

  /// Initialize the MLGO infrastructure (model runner and logger).
  /// Called from the constructor after parent initialization.
  /// Sets up:
  /// - NoInferenceModelRunner (logging only) or ModelUnderTrainingRunner
  /// - Logger if training log path is provided
  void initializeMLGO(const MachineSchedContext *C);

  /// Switch the logging context (e.g., when entering a new function).
  /// This allows the logger to separate training data by context.
  void switchContextForLog(StringRef Context);

  /// Log the reward for the current observation if logging is active.
  /// Should be called at the end of scheduling a region or function.
  void logRewardIfNeeded();

  /// Override pickNode to add ML-guided selection.
  /// Currently falls back to parent implementation (pure fallback mode).
  /// Future: will consult ML model for node selection hints.
  SUnit *pickNode(bool &IsTopNode) override;

  /// Extract features from a scheduling boundary's ready queue.
  /// Iterates over available candidates and extracts per-instruction features.
  void extractFeatures(SchedBoundary &Zone, const CandPolicy &ZonePolicy,
                       const RegPressureTracker &RPTracker, bool IsBottomUp);

  /// Extract per-instruction features for a single candidate.
  /// Populates feature tensors at the appropriate index based on position
  /// and queue (Top vs Bot).
  void extractCandidateFeatures(SchedCandidate &TryCand, int64_t Pos) const;

  /// Extract region-level features (register pressure limits).
  void extractGlobalFeatures();

  /// Log features and decision to training log.
  /// SchedIndex encodes both position and queue (2 * Pos + IsTopNode).
  void logMLFeatures(int64_t SchedIndex);

  /// Pick a node using the ML model's decision.
  /// Returns nullptr if the model returns an invalid index, signaling
  /// that the caller should fall back to heuristics.
  /// The model returns an index encoded as: 2 * Pos + IsTopNode
  /// where IsTopNode=1 means Top queue, IsTopNode=0 means Bot queue.
  SUnit *pickNodeByModel(bool &IsTopNode);

  /// Pick a node using the external inference server.
  /// Used in Online mode for online RL training.
  /// Returns nullptr if server returns an invalid index or connection fails.
  SUnit *pickNodeFromServer(bool &IsTopNode);

  /// Serialize current features to JSON for sending to inference server.
  json::Object serializeFeaturesToJSON() const;

  /// Scheduling modes for MLGO:
  /// - Default: No ML, use standard CoExec heuristics
  /// - Release: Use trained model for inference (read-only)
  /// - Development: Training mode with logging and model under training
  /// - Online: External inference server for online RL training
  enum class SchedMode : int { Default, Release, Development, Online };

protected:
  const MLModelRunner &getRunner() const { return *Runner; }

  // Machine Learning Guided Instruction Scheduling members
  std::unique_ptr<MLModelRunner> Runner;
  std::unique_ptr<Logger> Log;
  const SchedMode Mode;

  // External inference server client for Online mode
  std::unique_ptr<InferenceServerClient> InferenceClient;

private:
  /// Determine the scheduling mode based on command line options.
  static SchedMode determineMode();
};

/// Factory function to create a machine scheduler using MLCoExecSchedStrategy.
/// Used by GCNTargetMachine when "mlcoexec" strategy is selected.
ScheduleDAGInstrs *createGCNMLCoExecMachineScheduler(MachineSchedContext *C);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_AMDGPU_MLCOEXECSCHEDSTRATEGY_H
