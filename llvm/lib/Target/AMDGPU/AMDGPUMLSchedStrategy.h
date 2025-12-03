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

#include "GCNSchedStrategy.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {

class HardwareUnitInfo {
private:
  const MCProcResourceDesc *ProcRes = nullptr;
  // Ideally these would be sorted on how much they enable a secondary resource,
  // but that creates a chicken and egg problem and compile time explosion.
  SmallSetVector<SUnit *, 16> PrioritySUs;
  SmallSetVector<SUnit *, 16> AllSUs;
  unsigned TotalCycles = 0;

public:
  // TODO -- handle this better.
  bool IsAsync = false;
  unsigned Idx;

  HardwareUnitInfo(const MCProcResourceDesc *Res) : ProcRes(Res) {};
  HardwareUnitInfo() {}

  void setRes(const MCProcResourceDesc *Res) { ProcRes = Res; }

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
  const MCProcResourceDesc *getProcRes() { return ProcRes; }

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
    TotalCycles -= ReleaseAtCycle;
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
  }

  void print() {
    errs() << "HWUI has TotalCycles: " << getTotalCycles() << "\nIt contains SUs:\n";
    for (auto SU : AllSUs) {
      SU->getInstr()->dump();
    }
  }
};

class AMDGPUMLSchedStrategy final : public GCNSchedStrategy {
protected:
  bool tryCandidateBalanced(SchedCandidate &Cand, SchedCandidate &TryCand,
                            SchedBoundary *Zone);

  SmallVector<SUnit *, 16> SchedDSR;

  SmallVector<SUnit *, 16> SchedMFMA;

  SmallVector<SUnit *, 16> SchedEXP;

  SmallVector<HardwareUnitInfo, 8> HWUInfo;

  void collectUse();

  bool tryPendingCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                           SchedBoundary *Zone);

  void pickNodeFromQueue(SchedBoundary &Zone, const CandPolicy &ZonePolicy,
                         const RegPressureTracker &RPTracker,
                         SchedCandidate &Cand, bool &IsPending,
                         bool IsBottomUp);

  SUnit *pickNode(bool &IsTopNode) override;

public:
  AMDGPUMLSchedStrategy(const MachineSchedContext *C);

  void initialize(ScheduleDAGMI *DAG) override;

  void schedNode(SUnit *SU, bool IsTopNode) override;

  bool tryCriticalResource(SchedCandidate &TryCand, SchedCandidate &Cand,
                           SchedBoundary *Zone) const;

  bool tryCriticalResourceDependency(SchedCandidate &TryCand,
                                     SchedCandidate &Cand, SchedBoundary *Zone,
                                     bool IsAsyncPipe = false) const;

  unsigned getLatencyStallCycles(SUnit *SU, unsigned CurrCycle) const;

};

class AMDGPUMLPostSchedStrategy : public PostGenericScheduler {
protected:
  bool tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand) override;

public:
  AMDGPUMLPostSchedStrategy(const MachineSchedContext *C);
};

} // End namespace llvm