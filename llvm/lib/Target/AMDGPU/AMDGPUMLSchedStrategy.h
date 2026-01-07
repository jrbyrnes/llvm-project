//===-- AMDGPUMLSchedStrategy.h - ML-focused Scheduler Strategy -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// ML-focused scheduling strategy for AMDGPU.
//
//===----------------------------------------------------------------------===//

#include "GCNHazardRecognizer.h"
#include "GCNSchedStrategy.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/MachineCycleAnalysis.h"
#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {

//===----------------------------------------------------------------------===//
// Instruction Flavor Classification
//===----------------------------------------------------------------------===//

enum class InstructionFlavor : uint8_t {
  WMMA,            // WMMA/MFMA matrix operations
  SingleCycleVALU, // Single-cycle VALU (not TRANS32, not multi-cycle CVT)
  TRANS,           // Transcendental ops (v_exp, v_log, etc.) - 2 cycles
  MultiCycleVALU,  // 4-cycle CVT instructions (v_cvt_scalef32_pk8_fp8_f32)
  VMEM,            // FLAT/GLOBAL memory operations
  DS,              // LDS/GDS operations
  SALU,            // Scalar ALU
  DMA,             // Tensor DMA operations
  Fence,           // Fences and waits
  Other,           // Everything else
  NUM_FLAVORS,
  LLVM_MARK_AS_BITMASK_ENUM(/* LargestFlag = */ NUM_FLAVORS)
};

inline StringRef getFlavorName(InstructionFlavor F) {
  switch (F) {
  case InstructionFlavor::WMMA:           return "WMMA";
  case InstructionFlavor::SingleCycleVALU:return "VALU(1c)";
  case InstructionFlavor::TRANS:
    return "TRANS";
  case InstructionFlavor::MultiCycleVALU:
    return "VALU(Nc)";
  case InstructionFlavor::VMEM:           return "VMEM";
  case InstructionFlavor::DS:             return "DS";
  case InstructionFlavor::SALU:           return "SALU";
  case InstructionFlavor::DMA:            return "DMA";
  case InstructionFlavor::Fence:          return "Fence";
  case InstructionFlavor::Other:          return "Other";
  case InstructionFlavor::NUM_FLAVORS:    return "???";
  }
  llvm_unreachable("Unknown InstructionFlavor");
}

inline StringRef getFlavorShortName(InstructionFlavor F) {
  switch (F) {
  case InstructionFlavor::WMMA:           return "W";
  case InstructionFlavor::SingleCycleVALU:return "V";
  case InstructionFlavor::TRANS:
    return "T";
  case InstructionFlavor::MultiCycleVALU:
    return "C";
  case InstructionFlavor::VMEM:           return "M";
  case InstructionFlavor::DS:             return "D";
  case InstructionFlavor::SALU:           return "S";
  case InstructionFlavor::DMA:            return "X";
  case InstructionFlavor::Fence:          return "F";
  case InstructionFlavor::Other:          return "O";
  case InstructionFlavor::NUM_FLAVORS:    return "?";
  }
  llvm_unreachable("Unknown InstructionFlavor");
}

InstructionFlavor classifyFlavor(const MachineInstr *MI, const SIInstrInfo *SII);

using FlavorGroup = SmallVector<InstructionFlavor, 4>;

namespace FlavorGroups {
  inline FlavorGroup allVALU() {
    return {InstructionFlavor::SingleCycleVALU, InstructionFlavor::TRANS,
            InstructionFlavor::MultiCycleVALU};
  }
  inline FlavorGroup allMem() {
    return {InstructionFlavor::VMEM, InstructionFlavor::DS,
            InstructionFlavor::DMA};
  }
  inline FlavorGroup individual(InstructionFlavor F) {
    return {F};
  }
  inline FlavorGroup all() {
    FlavorGroup G;
    for (unsigned I = 0; I < static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS); ++I)
      G.push_back(static_cast<InstructionFlavor>(I));
    return G;
  }
}

/// AMDGPU-specific scheduling decision reasons. These provide more granularity
/// than the generic CandReason enum for debugging purposes.
enum class AMDGPUSchedReason : uint8_t {
  None,
  WMMACoexec,              // tryVALUCoexecSlot chose based on WMMA coexecution
  CritResourceBalance,     // tryCriticalResource chose based on resource pressure
  CritResourceDep,         // tryCriticalResourceDependency chose based on enabling
  // Shadow Mix: defer until shadow-filling instructions ready
  ShadowDeferWMMA,         // Deferred WMMA waiting for co-exec (VALU/DS)
  ShadowDeferTRANS32,      // Deferred TRANS32 waiting for VALU
  // Shadow Priority: prefer long-latency so short ones fill shadow
  ShadowPriorityWMMAOverDS,    // Prefer WMMA over DS (DS fills shadow)
  ShadowPriorityWMMAOverSALU,  // Prefer WMMA over SALU (SALU fills shadow)
  ShadowPriorityCVTOverDS,     // Prefer CVT over DS
  ShadowPriorityCVTOverSALU,   // Prefer CVT over SALU
  ShadowPriorityTRANS32OverVALU, // Prefer TRANS32 over 1c VALU
  ShadowPreferVALU1cOverSALUForTRANS, // Prefer VALU1c over SALU for TRANS shadow
  // Shadow Mix: prefer instruction that enables co-exec candidates
  ShadowEnableDirect,      // Directly enables needed co-exec flavor
  ShadowEnableLookahead,   // On path to enabling co-exec via lookahead
  NUM_REASONS
};

inline StringRef getReasonName(AMDGPUSchedReason R) {
  switch (R) {
  case AMDGPUSchedReason::None:              return "None";
  case AMDGPUSchedReason::WMMACoexec:        return "WMMACoexec";
  case AMDGPUSchedReason::CritResourceBalance: return "CritResource";
  case AMDGPUSchedReason::CritResourceDep:   return "CritResourceDep";
  case AMDGPUSchedReason::ShadowDeferWMMA:   return "ShadowDeferWMMA";
  case AMDGPUSchedReason::ShadowDeferTRANS32: return "ShadowDeferTRANS32";
  case AMDGPUSchedReason::ShadowPriorityWMMAOverDS:   return "ShadowWMMA>DS";
  case AMDGPUSchedReason::ShadowPriorityWMMAOverSALU: return "ShadowWMMA>SALU";
  case AMDGPUSchedReason::ShadowPriorityCVTOverDS:    return "ShadowCVT>DS";
  case AMDGPUSchedReason::ShadowPriorityCVTOverSALU:  return "ShadowCVT>SALU";
  case AMDGPUSchedReason::ShadowPriorityTRANS32OverVALU: return "ShadowTRANS32>VALU";
  case AMDGPUSchedReason::ShadowPreferVALU1cOverSALUForTRANS: return "ShadowVALU>SALU(TRANS)";
  case AMDGPUSchedReason::ShadowEnableDirect:    return "ShadowEnableDirect";
  case AMDGPUSchedReason::ShadowEnableLookahead: return "ShadowEnableLookahead";
  case AMDGPUSchedReason::NUM_REASONS:       return "???";
  }
  llvm_unreachable("Unknown AMDGPUSchedReason");
}

class RegionMixInfo {
public:
  static constexpr unsigned NumFlavors =
      static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS);

private:
  SmallVector<SmallVector<SUnit *, 8>, NumFlavors> AllSUs;

  SmallVector<SmallSetVector<SUnit *, 8>, NumFlavors> ScheduledSUs;

  SmallVector<DenseMap<SUnit *, unsigned>, NumFlavors> SUCycles;

  SmallVector<unsigned, NumFlavors> ReadyCounts;

  SmallVector<unsigned, NumFlavors> TotalCycles;

  SmallVector<unsigned, NumFlavors> ScheduledCycles;

public:
  void reset() {
    AllSUs.clear();
    AllSUs.resize(NumFlavors);
    ScheduledSUs.clear();
    ScheduledSUs.resize(NumFlavors);
    SUCycles.clear();
    SUCycles.resize(NumFlavors);
    ReadyCounts.assign(NumFlavors, 0);
    TotalCycles.assign(NumFlavors, 0);
    ScheduledCycles.assign(NumFlavors, 0);
  }

  void addSU(SUnit *SU, InstructionFlavor F, unsigned Cycles = 1) {
    unsigned Idx = static_cast<unsigned>(F);
    AllSUs[Idx].push_back(SU);
    TotalCycles[Idx] += Cycles;
    SUCycles[Idx][SU] = Cycles;
    if (SU->isTopReady())
      ReadyCounts[Idx]++;
  }

  void markScheduled(SUnit *SU, InstructionFlavor F) {
    unsigned Idx = static_cast<unsigned>(F);
    ScheduledSUs[Idx].insert(SU);
    ScheduledCycles[Idx] += SUCycles[Idx].lookup(SU);
    if (ReadyCounts[Idx] > 0)
      ReadyCounts[Idx]--;
  }

  void updateReadyCounts() {
    for (unsigned I = 0; I < NumFlavors; ++I) {
      ReadyCounts[I] = 0;
      for (SUnit *SU : AllSUs[I]) {
        if (!ScheduledSUs[I].contains(SU) && SU->isTopReady())
          ReadyCounts[I]++;
      }
    }
  }

  unsigned getReadyCount(InstructionFlavor F) const {
    return ReadyCounts[static_cast<unsigned>(F)];
  }

  unsigned getReadyCount(const FlavorGroup &G) const {
    unsigned Count = 0;
    for (InstructionFlavor F : G)
      Count += getReadyCount(F);
    return Count;
  }

  unsigned getPendingCount(InstructionFlavor F) const {
    unsigned Idx = static_cast<unsigned>(F);
    unsigned Total = AllSUs[Idx].size();
    unsigned Scheduled = ScheduledSUs[Idx].size();
    unsigned Ready = ReadyCounts[Idx];
    return Total - Scheduled - Ready;
  }

  unsigned getTotalCount(InstructionFlavor F) const {
    return AllSUs[static_cast<unsigned>(F)].size();
  }

  unsigned getRemainingCount(InstructionFlavor F) const {
    unsigned Idx = static_cast<unsigned>(F);
    return AllSUs[Idx].size() - ScheduledSUs[Idx].size();
  }

  unsigned getTotalCycles(InstructionFlavor F) const {
    return TotalCycles[static_cast<unsigned>(F)];
  }

  unsigned getRemainingCycles(InstructionFlavor F) const {
    unsigned Idx = static_cast<unsigned>(F);
    return TotalCycles[Idx] - ScheduledCycles[Idx];
  }

  ArrayRef<SUnit *> getSUs(InstructionFlavor F) const {
    return AllSUs[static_cast<unsigned>(F)];
  }

  void dumpMix(raw_ostream &OS, bool Detailed = false) const;

  void dumpReadyPending(raw_ostream &OS) const;
};

class HardwareUnitInfo {
private:
  // Ideally these would be sorted on how much they enable a secondary resource,
  // but that creates a chicken and egg problem and compile time explosion.
  SmallSetVector<SUnit *, 16> PrioritySUs;
  SmallSetVector<SUnit *, 16> AllSUs;
  unsigned TotalCycles = 0;
  InstructionFlavor Type;
  unsigned Exposed = 0;
  unsigned RemainingExposed = 0;

public:
  // TODO -- handle this better.
  bool IsAsync = false;
  unsigned Idx;
  bool IsIssueHideable = true;
  bool ProducesCoexecWindow = false;
  unsigned CoexecWindowSize = 0;

  HardwareUnitInfo() {}


  unsigned size() { return AllSUs.size(); }
  SUnit *getTargetSU() { return *PrioritySUs.begin(); }

  // TODO -- should we allow looking past the a single depth?
  SUnit *getNextTargetSU() {
    for (auto *PrioritySU : PrioritySUs) {
      if (!PrioritySU->isTopReady())
        return PrioritySU;
    }
    return nullptr;
  }

  unsigned getTotalCycles() { return TotalCycles; }

  void setType(unsigned TheType) {
    assert(TheType < (unsigned)InstructionFlavor::NUM_FLAVORS);
    Type = (InstructionFlavor)(TheType);
  }

  InstructionFlavor getType() const { return Type; }

  void setExposedCount(unsigned ExposedCount) {
    Exposed = ExposedCount;
    RemainingExposed = ExposedCount;
  }

  unsigned getRemainingExposed() { return RemainingExposed; }

  void reduceRemainingExposed() {
    if (RemainingExposed > 0)
      --RemainingExposed;
  }

  void insert(SUnit *SU, unsigned ReleaseAtCycle) {
    bool Inserted = AllSUs.insert(SU);
    TotalCycles += ReleaseAtCycle;

    assert(Inserted);
    if (PrioritySUs.empty()) {
      PrioritySUs.insert(SU);
      return;
    }
    unsigned SUDepth = SU->getDepth();
    unsigned CurrDepth = (*PrioritySUs.begin())->getDepth();
    if (SUDepth > CurrDepth)
      return;

    if (SUDepth == CurrDepth) {
      PrioritySUs.insert(SU);
      return;
    }

    // SU is lower depth and should be prioritized.
    PrioritySUs.clear();
    PrioritySUs.insert(SU);
  }

  bool contains(SUnit *SU) { return AllSUs.contains(SU); }

  bool isHigherPriority(SUnit *SU, SUnit *Other) {
    for (auto *SUOrder : PrioritySUs) {
      if (SUOrder == SU)
        return true;
      if (SUOrder == Other)
        return false;
    }

    return false;
  }

  void schedule(SUnit *SU, unsigned ReleaseAtCycle) {
    AllSUs.remove(SU);
    PrioritySUs.remove(SU);
    if (TotalCycles > ReleaseAtCycle)
      TotalCycles -= ReleaseAtCycle;
    else TotalCycles = 0;
    if (AllSUs.empty())
      return;
    if (PrioritySUs.empty()) {
      for (auto SU : AllSUs) {
        if (PrioritySUs.empty()) {
          PrioritySUs.insert(SU);
          continue;
        }
        unsigned SUDepth = SU->getDepth();
        unsigned CurrDepth = (*PrioritySUs.begin())->getDepth();
        if (SUDepth > CurrDepth)
          continue;

        if (SUDepth == CurrDepth) {
          PrioritySUs.insert(SU);
          continue;
        }

        // SU is lower depth and should be prioritized.
        PrioritySUs.clear();
        PrioritySUs.insert(SU);
      }
    }
  }

  void reset() {
    AllSUs.clear();
    PrioritySUs.clear();
    TotalCycles = 0;
    IsAsync = false;
    IsIssueHideable = true;
    Exposed = 0;
    RemainingExposed = 0;
    ProducesCoexecWindow = false;
    CoexecWindowSize = 0;
  }

  void print() {
    errs() << "HWUI: " << getFlavorName(Type) << "\n";
    errs() << "Count: " << AllSUs.size() << "\n";
    errs() << "TotalCycles: " << getTotalCycles() << "\n";
    errs() << "RemainingExposed: " << RemainingExposed << "\n";
    errs() << "IsIssueHideable: " << IsIssueHideable << "\n";
    errs() << "ProducesCoexecWindow: " << ProducesCoexecWindow << "\n";
  }
};

class AMDGPUMLSchedStrategy final : public GCNSchedStrategy {
protected:
  bool tryCandidateBalanced(SchedCandidate &Cand, SchedCandidate &TryCand,
                            SchedBoundary *Zone);

  SmallVector<SUnit *, 16> SchedDSR;

  SmallVector<SUnit *, 16> SchedMFMA;

  SmallVector<HardwareUnitInfo, 8> HWUInfo;

  SmallVector<SUnit *, 16> SchedTDM;

  SmallVector<SUnit *, 16> SchedEXP;

  RegionMixInfo MixInfo;

  AMDGPUSchedReason LastAMDGPUReason = AMDGPUSchedReason::None;

  unsigned FencedDSRLatency = 0;

  void collectUse();

  unsigned getHWUICyclesForInst(SUnit *SU, const SIInstrInfo *SII, unsigned ReleaseAtCycle);

  bool tryPendingCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                           SchedBoundary *Zone) override;

  void pickNodeFromQueue(SchedBoundary &Zone, const CandPolicy &ZonePolicy,
                         const RegPressureTracker &RPTracker,
                         SchedCandidate &Cand, bool &IsPending,
                         bool IsBottomUp);

  SUnit *pickNode(bool &IsTopNode) override;

  void dumpRegionSummary();

  void dumpPickSummary(SUnit *SU, bool IsTopNode, SchedCandidate &Cand);

public:
  AMDGPUMLSchedStrategy(const MachineSchedContext *C);

  void initialize(ScheduleDAGMI *DAG) override;

  void schedNode(SUnit *SU, bool IsTopNode) override;

  MachineCycleInfo CI;
};

class AMDGPUMLPostSchedStrategy : public PostGenericScheduler {
protected:
  bool CollectedUse = false;

  unsigned FencedDSRLatency = 0;

  SmallVector<SUnit *, 16> SchedDSR;

  SmallVector<SUnit *, 16> SchedMFMA;

  SmallVector<HardwareUnitInfo, 8> HWUInfo;

  SmallVector<SUnit *, 16> SchedTDM;

  SmallVector<SUnit *, 16> SchedEXP;

  RegionMixInfo MixInfo;

  AMDGPUSchedReason LastAMDGPUReason = AMDGPUSchedReason::None;

  void dumpRegionSummary();

  void dumpPickSummary(SUnit *SU, bool IsTopNode, SchedCandidate &Cand);

public:
  AMDGPUMLPostSchedStrategy(const MachineSchedContext *C);

  void schedNode(SUnit *SU, bool IsTopNode) override;

  bool tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                    SchedBoundary *Zone) override;

  bool tryPendingCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                           SchedBoundary *Zone);

  void collectUse();

  void enterRegion(MachineBasicBlock *bb, MachineBasicBlock::iterator begin,
                   MachineBasicBlock::iterator end, unsigned regioninstrs);

  unsigned getHWUICyclesForInst(SUnit *SU, const SIInstrInfo *SII,
                                unsigned ReleaseAtCycle);

  void initialize(ScheduleDAGMI *DAG) override;

  SUnit *pickNode(bool &IsTopNode) override;

  void pickNodeFromQueue(SchedBoundary &Zone, SchedCandidate &Cand,
                         bool &IsPending);
};

} // End namespace llvm