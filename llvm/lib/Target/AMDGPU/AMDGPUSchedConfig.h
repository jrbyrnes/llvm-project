//===- AMDGPUSchedConfig.h - Scheduler Configuration File -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// JSON-based configuration file for AMDGPU scheduling knobs.
/// An AI agent can write this file to tune the scheduler; the scheduler reads
/// it at construction time. Command-line cl::opt values override file values.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUSCHEDCONFIG_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUSCHEDCONFIG_H

#include "llvm/ADT/StringRef.h"
#include <optional>
#include <string>

namespace llvm {
namespace AMDGPU {

/// Parsed scheduling configuration from a JSON file.
/// All fields are optional — only fields present in the JSON are set.
struct SchedConfig {
  // --- GCNSchedStrategy knobs ---
  std::optional<unsigned> VGPRThresholdPercent;

  // --- CoExec knobs ---
  std::optional<std::string> CoexecExposedSort;
  std::optional<std::string> BlockCarriedLatency;
  std::optional<std::string> CoexecLookahead;
  std::optional<unsigned> ShadowMixNodeBudgetOverride;

  // --- Scoring mode ---
  std::optional<bool> UseScoring;

  // --- Scoring weights ---
  std::optional<double> WBiasPhysReg;
  std::optional<double> WRegExcess;
  std::optional<double> WStall;
  std::optional<double> WMissedSlots;
  std::optional<double> WMemoryPipeline;
  std::optional<double> WCoexecSlotAvoid;
  std::optional<double> WCoexecSlotPrefer;
  std::optional<double> WShadowMixDemandSatisfied;
  std::optional<double> WShadowMixDirectEnable;
  std::optional<double> WShadowMixLookahead;
  std::optional<double> WShadowMixGreedy;
  std::optional<double> WCritResource;
  std::optional<double> WCritResourceWMMAPrio;
  std::optional<double> WCritResourceDep;
  std::optional<double> WCritResourceDepLoopCarried;
  std::optional<double> WCritResourcePrio;
  std::optional<double> WCluster;
  std::optional<double> WWeak;
  std::optional<double> WRegMax;
  std::optional<double> WLatency;
  std::optional<double> WNodeOrder;
};

/// Parse a JSON scheduling configuration file at \p Path.
/// Returns a populated SchedConfig on success, or an empty SchedConfig if the
/// file does not exist or cannot be parsed (with a warning printed).
SchedConfig parseSchedConfig(StringRef Path);

/// \returns the path to the scheduling configuration file, resolved from the
/// command-line option or the default search path.
std::string getSchedConfigPath();

} // namespace AMDGPU
} // namespace llvm

#endif // LLVM_LIB_TARGET_AMDGPU_AMDGPUSCHEDCONFIG_H
