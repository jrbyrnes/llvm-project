//===- AMDGPUCoExecSchedStrategy.h - CoExec Scheduling Strategy -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Coexecution-focused scheduling strategy for AMDGPU.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECSCHEDSTRATEGY_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECSCHEDSTRATEGY_H

#include "GCNSchedStrategy.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {

namespace AMDGPU {
namespace DefaultBufferSizes {
constexpr unsigned DS = 16;
} // namespace DefaultBufferSizes

//===----------------------------------------------------------------------===//
// Instruction Flavor Classification
//===----------------------------------------------------------------------===//

enum class InstructionFlavor : uint8_t {
  WMMA,            // WMMA/MFMA matrix operations
  SingleCycleVALU, // Single-cycle VALU (not TRANS32, not multi-cycle CVT)
  TRANS,           // Transcendental ops (v_exp, v_log, etc.)
  MultiCycleVALU,  // VALU instructions with repeat rate > 1
  VMEM,            // FLAT/GLOBAL memory operations
  DS,              // LDS/GDS operations
  SALU,            // Scalar ALU
  DMA,             // Tensor DMA operations
  Fence,           // Fences and waits
  Other,           // Everything else
  NUM_FLAVORS
};

inline StringRef getFlavorName(InstructionFlavor F) {
  switch (F) {
  case InstructionFlavor::WMMA:
    return "WMMA";
  case InstructionFlavor::SingleCycleVALU:
    return "VALU(1c)";
  case InstructionFlavor::TRANS:
    return "TRANS";
  case InstructionFlavor::MultiCycleVALU:
    return "VALU(Nc)";
  case InstructionFlavor::VMEM:
    return "VMEM";
  case InstructionFlavor::DS:
    return "DS";
  case InstructionFlavor::SALU:
    return "SALU";
  case InstructionFlavor::DMA:
    return "DMA";
  case InstructionFlavor::Fence:
    return "Fence";
  case InstructionFlavor::Other:
    return "Other";
  case InstructionFlavor::NUM_FLAVORS:
    return "???";
  }
  llvm_unreachable("Unknown InstructionFlavor");
}

inline StringRef getFlavorShortName(InstructionFlavor F) {
  switch (F) {
  case InstructionFlavor::WMMA:
    return "W";
  case InstructionFlavor::SingleCycleVALU:
    return "V";
  case InstructionFlavor::TRANS:
    return "T";
  case InstructionFlavor::MultiCycleVALU:
    return "C";
  case InstructionFlavor::VMEM:
    return "M";
  case InstructionFlavor::DS:
    return "D";
  case InstructionFlavor::SALU:
    return "S";
  case InstructionFlavor::DMA:
    return "X";
  case InstructionFlavor::Fence:
    return "F";
  case InstructionFlavor::Other:
    return "O";
  case InstructionFlavor::NUM_FLAVORS:
    return "?";
  }
  llvm_unreachable("Unknown InstructionFlavor");
}

InstructionFlavor classifyFlavor(const MachineInstr &MI,
                                 const SIInstrInfo &SII);

using FlavorGroup = SmallVector<InstructionFlavor, 4>;

namespace FlavorGroups {
inline FlavorGroup allALU() {
  return {InstructionFlavor::SingleCycleVALU, InstructionFlavor::TRANS,
          InstructionFlavor::MultiCycleVALU, InstructionFlavor::WMMA,
          InstructionFlavor::SALU};
}
inline FlavorGroup allVALU() {
  return {InstructionFlavor::SingleCycleVALU, InstructionFlavor::TRANS,
          InstructionFlavor::MultiCycleVALU};
}
inline FlavorGroup allMem() {
  return {InstructionFlavor::VMEM, InstructionFlavor::DS,
          InstructionFlavor::DMA};
}
inline FlavorGroup individual(InstructionFlavor F) { return {F}; }
inline FlavorGroup all() {
  FlavorGroup G;
  for (unsigned I = 0;
       I < static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS); ++I)
    G.push_back(static_cast<InstructionFlavor>(I));
  return G;
}
} // namespace FlavorGroups

/// AMDGPU-specific scheduling decision reasons. These provide more granularity
/// than the generic CandReason enum for debugging purposes.
enum class AMDGPUSchedReason : uint8_t {
  None,
  CritResourceBalance, // tryCriticalResource chose based on resource pressure
  CritResourceDep,     // tryCriticalResourceDependency chose based on enabling
  NUM_REASONS
};

inline StringRef getReasonName(AMDGPUSchedReason R) {
  switch (R) {
  case AMDGPUSchedReason::None:
    return "None";
  case AMDGPUSchedReason::CritResourceBalance:
    return "CritResource";
  case AMDGPUSchedReason::CritResourceDep:
    return "CritResourceDep";
  case AMDGPUSchedReason::NUM_REASONS:
    return "???";
  }
  llvm_unreachable("Unknown AMDGPUSchedReason");
}

} // End namespace AMDGPU

//===----------------------------------------------------------------------===//
// Hardware Unit Information
//===----------------------------------------------------------------------===//

/// Certain ALU instructions can coexecute with others -- we have coexecution
/// window "producers" and coexecution window "consumers". The producers produce
/// a shadow in which the consumers can hide their execution. The CoexecSlot
/// models part of the shadow for the coexecution window producers.
struct CoexecSlot {
  /// Upper 16 bits are the preferred flavors, and lower 16 bits are the allowed
  /// flavors
  static const unsigned Lanewidth = 16;
  /// Mask of preferred and allowed flavors.
  unsigned Flavors = 0;
  /// How many cycles of the shadow this coexec slot represents.
  unsigned Cycles = 0;

  /// Add a \p Flavor to the Flavors. If \p IsPreferred is true, it will add it
  /// to the preferred flavors, otherwise it will just add to the allowed
  /// flavors.
  void addFlavor(AMDGPU::InstructionFlavor Flavor, bool IsPreferred = false) {
    assert((int)AMDGPU::InstructionFlavor::NUM_FLAVORS <= Lanewidth);
    unsigned FlavorOffset = 1 << (unsigned)Flavor;
    Flavors |= FlavorOffset;
    if (IsPreferred) {
      unsigned PreferredOffset = FlavorOffset << Lanewidth;
      Flavors |= PreferredOffset;
    }
  }

  void dump() {
    for (unsigned I = 0; I < (unsigned)AMDGPU::InstructionFlavor::NUM_FLAVORS;
         I++) {
      if (Flavors && I) {
        dbgs() << "Holds: "
               << AMDGPU::getFlavorName((AMDGPU::InstructionFlavor)I) << "\n";
      }
      if (Flavors && (I << Lanewidth)) {
        dbgs() << "  Is Preferred Flavor\n";
      }
    }
    dbgs() << "Total Cycles: " << Cycles << "\n";
  }

  /// Check if \p Flavor is coexecutable in this slot. If \p Preferred is true,
  /// then \returns if it is a preferred flavor. Otherwise, \p returns if it is
  /// an allowed flavor.
  static bool canHold(AMDGPU::InstructionFlavor Flavor, unsigned FlavorMask,
                      bool Preferred) {
    return FlavorMask &
           ((1 << (unsigned)Flavor) << (Preferred ? Lanewidth : 0));
  }
};

/// HardwareUnitInfo is a wrapper class which maps to some real hardware
/// resource. This is used to model hardware resource pressure per region, and
/// guide scheduling heuristics.
class HardwareUnitInfo {
private:
  /// PrioritySUs maintains a list of the SUs we want to prioritize scheduling
  /// for this HardwareUnit. This is used for agreement between
  /// tryCriticalResourceDependency and tryCriticalResource: we schedule the
  /// dependencies for a SU on critical resource, then schedule that same SU on
  /// the critical resource. This agreement results in shorter live ranges and
  /// more regular HardwareUnit access patterns. SUs are prioritized based on
  /// depth for top-down scheduling.
  SmallSetVector<SUnit *, 16> PrioritySUs;
  /// All the SUs in the region that consume this resource
  SmallSetVector<SUnit *, 16> AllSUs;
  /// All the SUs for this HardwareUnit that have already been scheduled.
  SmallVector<SUnit *, 16> ScheduledSUs;
  /// The total number of busy cycles for this HardwareUnit for a given region.
  unsigned TotalCycles = 0;
  // InstructionFlavor mapping
  AMDGPU::InstructionFlavor Type;
  // Idx mappuing
  unsigned Idx;
  // Whether or not instructions on this HardwareUnit may produce a window in
  // which instructions in other HardwareUnits can coexecute. For example, WMMA
  // MFMA instructions may take multiple cycles, which may be overlapped with
  // instructions on other HardwareUnits
  bool ProducesCoexecWindow = false;
  /// How many instructons can be held simultaneously for this HardwareUnit.
  /// A value of 0 or 1 means that there is no buffer.
  unsigned BufferSize = 0;
  /// How many cycles it takes for an instruction to clear the buffer.
  unsigned BufferCycles = 0;
  /// The lower bound of number of instructions on this HardwareUnit which can
  /// not be hidden.
  unsigned ExposedCount = 0;
  /// The lower bound of number of cycles on this HardwareUnit which can not be
  /// hidden.
  unsigned ExpectedTopLineCycles = 0;
  /// The upper bound of number of instructions on this HardwareUnit which can
  /// be hidden.
  unsigned FullyHiddenCount = 0;
  /// The shadow cycles that instructions on this HardwareUnit produce. This
  /// maps a flavormasks to cyclecount. computeCoexecCycles uses this map to
  /// find the optimal coexecution latency hiding. To do this, it modifies the
  /// map. After computeCoexecCycles, the cycle counts in CoexecCycles will be
  /// those that can not be hidden.
  DenseMap<unsigned, unsigned> CoexecCycles;

public:
  HardwareUnitInfo() {}

  unsigned size() { return AllSUs.size(); }

  unsigned getTotalCycles() { return TotalCycles; }

  void setType(unsigned TheType) {
    assert(TheType < (unsigned)AMDGPU::InstructionFlavor::NUM_FLAVORS);
    Type = (AMDGPU::InstructionFlavor)(TheType);
  }

  AMDGPU::InstructionFlavor getType() const { return Type; }

  unsigned getIdx() const { return Idx; }

  bool producesCoexecWindow() const { return ProducesCoexecWindow; }

  void setProducesCoexecWindow(bool Val) { ProducesCoexecWindow = Val; }

  bool contains(SUnit *SU) { return AllSUs.contains(SU); }

  void setBufferSize(unsigned Size) { BufferSize = Size; }

  unsigned getBufferSize() { return BufferSize; }

  void setExposedCount(unsigned Exposed) { ExposedCount = Exposed; }

  unsigned getExpectedTopLineCycles() { return ExpectedTopLineCycles; }

  /// Reduce the ExpectedTopLineCycles by \p Reduction amount.
  void reduceExpectedTopLineCycles(unsigned Reduction) {
    assert(Reduction <= ExpectedTopLineCycles);
    ExpectedTopLineCycles -= Reduction;
  }

  /// Find slots that can hide instructions in the \p Other HardwareUnit.
  /// Reduce the cycle count of these slots, and reduce the exposed count
  /// of the \p Other HardwareUnit. If the \p Other HardwareUnit has
  /// coexecslots, reduce the count of each coexecslot by one, as this
  /// slot is no longer available for coexecution.
  void hide(HardwareUnitInfo *Other, bool Preferred) {
    // How many cycles we were able to hide
    unsigned TotalHiddenCycles = 0;
    // How many cycles in the other hardware unit that still need to be hidden.
    unsigned RemainingCycles = Other->ExpectedTopLineCycles;
    unsigned TotalCycles = RemainingCycles;
    for (auto &Slot : CoexecCycles) {
      if (!CoexecSlot::canHold(Other->getType(), Slot.first, Preferred))
        continue;

      unsigned &SlotCycles = Slot.second;
      // Loop until we run out of instructions to hide in the other HarwareUnit,
      // or until we run out of slots in which to hide them.
      while (RemainingCycles && SlotCycles) {
        SlotCycles -= 1;
        // If the other HardwareUnit has coexec slots, remove one of each as
        // we have consumed the instruction.
        unsigned Latency = Other->hideInstruction();
        assert(Latency <= RemainingCycles);
        RemainingCycles -= Latency;
        TotalHiddenCycles += Latency;
        assert(TotalHiddenCycles <= TotalCycles);
      }
      if (TotalHiddenCycles == TotalCycles)
        break;
    }
    Other->ExpectedTopLineCycles = RemainingCycles;
  }

  /// Check if this HardwareUnit can hide the \p Other . If \p Preferred
  /// is true, \p returns whether the \p Other flavor is preferred. Otherwise,
  /// \p returns if it is allowed in the shadow.
  bool canHide(AMDGPU::InstructionFlavor Other, bool Preferred) {
    for (auto SlotMask : CoexecCycles.keys()) {
      if (CoexecSlot::canHold(Other, SlotMask, Preferred))
        return true;
    }
    return false;
  }

  bool isALU() {
    auto AllALU = AMDGPU::FlavorGroups::allALU();
    for (auto Flavor : AllALU) {
      if (Type == Flavor)
        return true;
    }
    return false;
  }

  /// Adjust the state to account for one hidden instruction. \p returns the
  /// number of cycles for this hidden instruction. This is the issue cycle plus
  /// any coexec cycles that the instruction produced. Increments the
  /// FullyHiddenCount.
  unsigned hideInstruction() {
    unsigned ReducedCount = 1;
    ++FullyHiddenCount;
    assert(FullyHiddenCount <= AllSUs.size());
    for (auto &Slot : CoexecCycles) {
      if (Slot.second > 0) {
        Slot.second -= 1;
        ++ReducedCount;
      }
    }
    return ReducedCount;
  }

  unsigned getFullyHiddenCount() { return FullyHiddenCount; }

  /// \returns the next cycle where there is space in the buffer.
  unsigned getBufferAvailableCycle(unsigned CurrCycle) {
    // There is no buffer.
    if (BufferSize <= 1)
      return CurrCycle;

    // Buffer is available now.
    if (ScheduledSUs.size() < BufferSize)
      return CurrCycle;

    return BufferCycles +
           ScheduledSUs[ScheduledSUs.size() - BufferSize]->TopReadyCycle;
  }

  /// \returns trrue if there is a difference in priority between \p SU and \p
  /// Other. If so, \returns the SUnit with higher priority. This
  /// method looks through the PrioritySUs to dtermine if one SU is more
  /// prioritized than the other. If neither are in the PrioritySUs list, then
  /// neither have priority over each other.
  SUnit *getHigherPriority(SUnit *SU, SUnit *Other) {
    for (auto *SUOrder : PrioritySUs) {
      if (SUOrder == SU) {
        return SU;
      }
      if (SUOrder == Other) {
        return Other;
      }
    }
    return nullptr;
  }

  void reset() {
    AllSUs.clear();
    PrioritySUs.clear();
    CoexecCycles.clear();
    TotalCycles = 0;
    ProducesCoexecWindow = false;
    BufferSize = 0;
    BufferCycles = 0;
    ExposedCount = 0;
    ExpectedTopLineCycles = 0;
    FullyHiddenCount = 0;
  }

  /// \returns the next SU in PriortySUs that is not ready. If \p LookDeep is
  /// set, we will look beyond the PrioritySUs (if all the PrioritSUs are ready)
  /// to AllSUs to attempt to find a target SU. When looking through AllSUs we
  /// sort pick the target SU by minimal depth for top-down scheduling.
  /// getNextTargetSU is useful for determining which SU on this HardwareUnit we
  /// are trying to schedule - this info helps us determine which dependencies
  /// to schedule. LookDeep is useful if the dependencies are long latency (e.g.
  /// memory instructions). If we have many lkong latency dependencies, it is
  /// beneficial to enable SUs multiple levels ahead.
  SUnit *getNextTargetSU(bool LookDeep = false);
  /// insert the \p SU into the AllSUs and account its \p BlockingCycles into
  /// the TotalCycles. The \p CoexecSlots are added parsed an accumulated into
  /// the CoexecCycles. This maintains the list of PrioritySUs.
  void insert(SUnit *SU, unsigned BlockingCycles,
              SmallVectorImpl<CoexecSlot> &CoexecSlots);
  /// schedule the \p SU by removing it from the AllSus and reducing its \p
  /// BlockingCycles from the TotalCycles. This maintains the list of
  /// PrioritySUS.
  void schedule(SUnit *SU, unsigned BlockingCycles);
  /// After we've collected all the region pressure for this HWUI, correct for
  /// any specifics of the behavior of this resource. For example, if we the
  /// HardwareUnit can hold N instructions simultaneously, then there is no
  /// penalty for scheduling N instructions back to back.
  void finalizeCycles();

  void dumpCoexecCycles() {
    for (auto &Entry : CoexecCycles) {
      dbgs() << "    HardwareUnit has coexec slot: \n";
      CoexecSlot TempSlot;
      TempSlot.Flavors = Entry.first;
      TempSlot.Cycles = Entry.second;
      TempSlot.dump();
    }
  }
};

//===----------------------------------------------------------------------===//
// Candidate Heuristics
//===----------------------------------------------------------------------===//

/// CandidateHeuristics contains state and implementations to facilitate making
/// per instruction scheduling decisions; it contains methods used in
/// tryCandidate to decide which instruction to schedule next.
class CandidateHeuristics {
protected:
  ScheduleDAGMI *DAG;
  const SIInstrInfo *SII;
  const SIRegisterInfo *SRI;
  const TargetSchedModel *SchedModel;
  SmallVector<HardwareUnitInfo, 8> HWUInfo;

  /// Walk over the region and collect total usage per HardwareUnit
  void collectHWUIPressure();

  /// Compute the blocking cycles for the appropriate HardwareUnit given an \p
  /// SU
  unsigned getHWUICyclesForInst(SUnit *SU);

  /// Given a \p SU , calculate the c
  bool getCoexecSlots(SUnit *SU, SmallVectorImpl<CoexecSlot> &CoexecSlots);

  /// Caculate how many cycles for each HardwareUnit can be hidden behind
  /// execution on another HardwareUnit. This computation is done by simplifying
  /// the problem and arrives at estimates to guide the heuristics. One obvious
  /// simplification is that we ignore dependencies that may make coexecution
  /// impossible. Ultimately, this analysis will produce the estimated exposed
  /// count per HardwareUnit. Understanding the hidden / exposed counts helps us
  /// make better decisions about which types of instructions to attempt to hide
  /// behind other HardwareUnits.
  void computeCoexecCycles();

public:
  CandidateHeuristics() = default;

  void initialize(ScheduleDAGMI *DAG, const TargetSchedModel *SchedModel,
                  const TargetRegisterInfo *TRI);

  /// Given a \p Flavor , find the corresponding HardwareUnit. \returns the
  /// mapped HardwareUnit.
  HardwareUnitInfo *getHWUIFromFlavor(AMDGPU::InstructionFlavor Flavor);

  void schedNode(SUnit *SU);

  /// Sort the HardwarUnitInfo vector. After sorting, the HWUI that are highest
  /// priority are first. Priority is determined by maximizing coexecution and
  /// keeping the critical Hardware unit busy.
  void sortHWUIResources();

  // Sort the HardwarUnitInfo vector such that HWUI appearing earlier cannot be
  // hidden by HWUI appearing later.
  void sortHWUIResourcesByCoexecution();

  /// Check for critical resource consumption. Prefer the candidate that uses
  /// the most prioritized HardwareUnit. If both candidates use the same
  /// HarwareUnit, prefer the candidate with higher priority on that
  /// HardwareUnit.
  bool tryCriticalResource(GenericSchedulerBase::SchedCandidate &TryCand,
                           GenericSchedulerBase::SchedCandidate &Cand,
                           SchedBoundary *Zone) const;

  /// Check for dependencies of instructions that use prioritized HardwareUnits.
  /// Prefer the candidate that is a dependency of an instruction that uses the
  /// most prioritized HardwareUnit. If both candidates enable the same
  /// HardwareUnit, prefer the candidate that enables the higher priority
  /// instruction on that HardwareUnit.
  bool
  tryCriticalResourceDependency(GenericSchedulerBase::SchedCandidate &TryCand,
                                GenericSchedulerBase::SchedCandidate &Cand,
                                SchedBoundary *Zone) const;

  void dumpRegionSummary();
};

class AMDGPUCoExecSchedStrategy final : public GCNSchedStrategy {
protected:
  bool tryEffectiveStall(SchedCandidate &Cand, SchedCandidate &TryCand,
                         SchedBoundary &Zone);
  AMDGPU::AMDGPUSchedReason LastAMDGPUReason = AMDGPU::AMDGPUSchedReason::None;
  CandidateHeuristics Heurs;

  void dumpPickSummary(SUnit *SU, bool IsTopNode, SchedCandidate &Cand);
  bool tryCandidateCoexec(SchedCandidate &Cand, SchedCandidate &TryCand,
                          SchedBoundary *Zone);
  void pickNodeFromQueue(SchedBoundary &Zone, const CandPolicy &ZonePolicy,
                         const RegPressureTracker &RPTracker,
                         SchedCandidate &Cand, bool &PickedPending,
                         bool IsBottomUp);

public:
  AMDGPUCoExecSchedStrategy(const MachineSchedContext *C);

  void initPolicy(MachineBasicBlock::iterator Begin,
                  MachineBasicBlock::iterator End,
                  unsigned NumRegionInstrs) override;
  void initialize(ScheduleDAGMI *DAG) override;
  SUnit *pickNode(bool &IsTopNode) override;
  void schedNode(SUnit *SU, bool IsTopNode) override;
};

ScheduleDAGInstrs *createGCNCoExecMachineScheduler(MachineSchedContext *C);
ScheduleDAGInstrs *createGCNNoopPostMachineScheduler(MachineSchedContext *C);

} // End namespace llvm

#endif // LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECSCHEDSTRATEGY_H
