//===- AMDGPUSchedConfig.cpp - Scheduler Configuration File ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPUSchedConfig.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-sched-config"

static cl::opt<std::string> SchedConfigFile(
    "amdgpu-sched-config", cl::Hidden,
    cl::desc("Path to a JSON scheduling configuration file. Knobs specified "
             "in this file are applied unless overridden by command-line "
             "options."));

std::string AMDGPU::getSchedConfigPath() { return SchedConfigFile; }

/// Try to read an optional double from a JSON object.
static std::optional<double> getOptionalDouble(const json::Object &Obj,
                                               StringRef Key) {
  const json::Value *V = Obj.get(Key);
  if (!V)
    return std::nullopt;
  if (auto D = V->getAsNumber())
    return *D;
  LLVM_DEBUG(dbgs() << "amdgpu-sched-config: key '" << Key
                    << "' is not a number, ignoring\n");
  return std::nullopt;
}

/// Try to read an optional unsigned from a JSON object.
static std::optional<unsigned> getOptionalUnsigned(const json::Object &Obj,
                                                   StringRef Key) {
  const json::Value *V = Obj.get(Key);
  if (!V)
    return std::nullopt;
  if (auto I = V->getAsUINT64())
    return static_cast<unsigned>(*I);
  if (auto I = V->getAsInteger())
    return static_cast<unsigned>(*I);
  LLVM_DEBUG(dbgs() << "amdgpu-sched-config: key '" << Key
                    << "' is not an integer, ignoring\n");
  return std::nullopt;
}

/// Try to read an optional string from a JSON object.
static std::optional<std::string> getOptionalString(const json::Object &Obj,
                                                    StringRef Key) {
  const json::Value *V = Obj.get(Key);
  if (!V)
    return std::nullopt;
  if (auto S = V->getAsString())
    return S->str();
  LLVM_DEBUG(dbgs() << "amdgpu-sched-config: key '" << Key
                    << "' is not a string, ignoring\n");
  return std::nullopt;
}

/// Try to read an optional bool from a JSON object.
static std::optional<bool> getOptionalBool(const json::Object &Obj,
                                           StringRef Key) {
  const json::Value *V = Obj.get(Key);
  if (!V)
    return std::nullopt;
  if (auto B = V->getAsBoolean())
    return *B;
  LLVM_DEBUG(dbgs() << "amdgpu-sched-config: key '" << Key
                    << "' is not a boolean, ignoring\n");
  return std::nullopt;
}

AMDGPU::SchedConfig AMDGPU::parseSchedConfig(StringRef Path) {
  SchedConfig Cfg;

  if (Path.empty())
    return Cfg;

  auto BufOrErr = MemoryBuffer::getFile(Path);
  if (!BufOrErr) {
    LLVM_DEBUG(dbgs() << "amdgpu-sched-config: cannot open '" << Path
                      << "': " << BufOrErr.getError().message() << "\n");
    return Cfg;
  }

  auto Parsed = json::parse(BufOrErr.get()->getBuffer());
  if (!Parsed) {
    LLVM_DEBUG(dbgs() << "amdgpu-sched-config: JSON parse error in '" << Path
                      << "': " << toString(Parsed.takeError()) << "\n");
    return Cfg;
  }

  const json::Object *Root = Parsed->getAsObject();
  if (!Root) {
    LLVM_DEBUG(dbgs() << "amdgpu-sched-config: root is not a JSON object\n");
    return Cfg;
  }

  LLVM_DEBUG(dbgs() << "amdgpu-sched-config: loaded config from '" << Path
                    << "'\n");

  // GCNSchedStrategy knobs
  Cfg.VGPRThresholdPercent = getOptionalUnsigned(*Root, "VGPRThresholdPercent");

  // CoExec knobs
  Cfg.CoexecExposedSort = getOptionalString(*Root, "CoexecExposedSort");
  Cfg.BlockCarriedLatency = getOptionalString(*Root, "BlockCarriedLatency");
  Cfg.CoexecLookahead = getOptionalString(*Root, "CoexecLookahead");
  Cfg.ShadowMixNodeBudgetOverride =
      getOptionalUnsigned(*Root, "ShadowMixNodeBudgetOverride");

  // Scoring mode
  Cfg.UseScoring = getOptionalBool(*Root, "UseScoring");

  // Scoring weights
  Cfg.WBiasPhysReg = getOptionalDouble(*Root, "WBiasPhysReg");
  Cfg.WRegExcess = getOptionalDouble(*Root, "WRegExcess");
  Cfg.WStall = getOptionalDouble(*Root, "WStall");
  Cfg.WMissedSlots = getOptionalDouble(*Root, "WMissedSlots");
  Cfg.WMemoryPipeline = getOptionalDouble(*Root, "WMemoryPipeline");
  Cfg.WCoexecSlotAvoid = getOptionalDouble(*Root, "WCoexecSlotAvoid");
  Cfg.WCoexecSlotPrefer = getOptionalDouble(*Root, "WCoexecSlotPrefer");
  Cfg.WShadowMixDemandSatisfied =
      getOptionalDouble(*Root, "WShadowMixDemandSatisfied");
  Cfg.WShadowMixDirectEnable =
      getOptionalDouble(*Root, "WShadowMixDirectEnable");
  Cfg.WShadowMixLookahead = getOptionalDouble(*Root, "WShadowMixLookahead");
  Cfg.WShadowMixGreedy = getOptionalDouble(*Root, "WShadowMixGreedy");
  Cfg.WCritResource = getOptionalDouble(*Root, "WCritResource");
  Cfg.WCritResourceWMMAPrio =
      getOptionalDouble(*Root, "WCritResourceWMMAPrio");
  Cfg.WCritResourceDep = getOptionalDouble(*Root, "WCritResourceDep");
  Cfg.WCritResourceDepLoopCarried =
      getOptionalDouble(*Root, "WCritResourceDepLoopCarried");
  Cfg.WCritResourcePrio = getOptionalDouble(*Root, "WCritResourcePrio");
  Cfg.WCluster = getOptionalDouble(*Root, "WCluster");
  Cfg.WWeak = getOptionalDouble(*Root, "WWeak");
  Cfg.WRegMax = getOptionalDouble(*Root, "WRegMax");
  Cfg.WLatency = getOptionalDouble(*Root, "WLatency");
  Cfg.WNodeOrder = getOptionalDouble(*Root, "WNodeOrder");

  return Cfg;
}
