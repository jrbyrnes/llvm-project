//===-- AMDGPUMLSchedStrategy.cpp - ML-focused Scheduler Strategy ---------===//
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

#include "AMDGPUMLSchedStrategy.h"
#include "llvm/CodeGen/MachineScheduler.h"

#define DEBUG_TYPE "machine-scheduler"

using namespace llvm;

static cl::opt<unsigned> ResourcesToBalance(
    "amdgpu-resource-balancing", cl::Hidden,
    cl::desc("Number of resources we will try to balance during scheduling."),
    cl::init(100));

static cl::opt<unsigned> DSLatency(
    "amdgpu-ds-fifo-latency", cl::Hidden,
    cl::desc("Hazard latency of DS_LOAD FIFO Full."),
    cl::init(40));

static cl::opt<bool> IgnoreVALU(
  "amdgpu-ignore-valu-resource-balancing", cl::Hidden,
  cl::desc("Whether or not to ignore VALU unit when balancing HW resoiurces."),
  cl::init(true));

AMDGPUMLSchedStrategy::AMDGPUMLSchedStrategy(const MachineSchedContext *C)
    : GCNSchedStrategy(C) {
  SchedStages.push_back(GCNSchedStageID::ILPInitialSchedule);
  SchedStages.push_back(GCNSchedStageID::PreRARematerialize);
  // Use more accurate GCN pressure trackers.
  UseGCNTrackers = false;
}

void AMDGPUMLSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  GCNSchedStrategy::initialize(DAG);

  const MCSchedModel &SM = MF->getSubtarget().getSchedModel();
  unsigned NumPR = SM.getNumProcResourceKinds();
  HWUInfo.resize(NumPR);
  for (unsigned I = 0; I < NumPR; I++) {
    HWUInfo[I].setRes(SM.getProcResource(I));
    HWUInfo[I].Idx = I;
  }
  if (NumPR > 8)
    HWUInfo[8].IsAsync = true;
  
  CI.compute(*MF);
}

static bool shouldCheckPending(SchedBoundary &Zone,
                               const TargetSchedModel *SchedModel) {
  return true;

  // FIXME -- enable this method, need to share flag
  // bool HasBufferedModel =
  //    SchedModel->hasInstrSchedModel() && SchedModel->getMicroOpBufferSize();
  // unsigned Combined = Zone.Available.size() + Zone.Pending.size();
  // return true; //Combined <= PendingQueueLimit && HasBufferedModel;
}

static SUnit *pickOnlyChoice(SchedBoundary &Zone,
                             const TargetSchedModel *SchedModel) {
  // pickOnlyChoice() releases pending instructions and checks for new hazards.
  SUnit *OnlyChoice = Zone.pickOnlyChoice();
  if (!shouldCheckPending(Zone, SchedModel) || Zone.Pending.empty())
    return OnlyChoice;

  return nullptr;
}

void AMDGPUMLSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  auto MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  if (SchedModel && SchedModel->hasInstrSchedModel()) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {

      unsigned I = 0;
      bool FoundIt = false;
      for (; I < HWUInfo.size(); I++) {
        if (HWUInfo[I].Idx == PI->ProcResourceIdx) {
          FoundIt = true;
          break;
        }
      }
      assert(FoundIt);
      HWUInfo[I].schedule(SU, PI->ReleaseAtCycle);
    }

    if (SII->isMFMAorWMMA(*MI)) {
      SchedMFMA.push_back(SU);
    }
    if (SII->isDS(*MI) && MI->mayLoad()) {
      SchedDSR.push_back(SU);
    }
    if (SII->isTRANS(*MI)) {
      SchedEXP.push_back(SU);
    }

    auto Opc = MI->getOpcode();
    if (Opc == AMDGPU::ATOMIC_FENCE || Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2) {
      SchedTDM.push_back(SU);
    }
  }

  GCNSchedStrategy::schedNode(SU, IsTopNode);
}

void AMDGPUMLSchedStrategy::collectUse() {
  CollectedUse = true;
  SchedDSR.clear();
  SchedMFMA.clear();
  SchedEXP.clear();
  SchedTDM.clear();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  for (auto &HWUI : HWUInfo) {
    HWUI.reset();
  }

  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  for (auto &SU : DAG->SUnits) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(&SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      auto Opc = SU.getInstr()->getOpcode();
      bool IsDMA = Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2 ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2_gfx1250 ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS_gfx1250 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_gfx1250 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR_gfx1250;
      unsigned Latency = IsDMA ? SU.Latency : PI->ReleaseAtCycle;
      if (SII->isDS(*SU.getInstr()) && SU.getInstr()->mayLoad())
        Latency = DSLatency;
      HWUInfo[PI->ProcResourceIdx].insert(&SU, Latency);
    }
  }


  HWUInfo[4].reset();
  if (IgnoreVALU)
    HWUInfo[7].reset();

}

static void sortResources(SmallVectorImpl<HardwareUnitInfo> &HWUInfo) {
  // Highest priority should be first.
  sort(HWUInfo, [](HardwareUnitInfo &A, HardwareUnitInfo &B) {
    // The most demanded resource is the highest priority
    if (A.getTotalCycles() != B.getTotalCycles())
      return A.getTotalCycles() > B.getTotalCycles();

    // In ties -- prefer the resource with longer latency instructions
    if (A.size() != B.size())
      return A.size() < B.size();

    // Default to HardwareUnitInfo order
    return A.Idx < B.Idx;
  });
}

bool AMDGPUMLSchedStrategy::tryCriticalResource(SchedCandidate &TryCand,
                                                SchedCandidate &Cand,
                                                SchedBoundary *Zone) const {

  unsigned CandOp = Cand.SU->getInstr()->getOpcode();
  bool CandIsLoad = CandOp == AMDGPU::TENSOR_LOAD_TO_LDS_D2 || CandOp == AMDGPU::S_WAIT_TENSORCNT || CandOp == AMDGPU::S_BARRIER_WAIT || CandOp == AMDGPU::S_BARRIER_SIGNAL_IMM;
  if (CandIsLoad) {
    if (Cand.Reason > RegCritical)
      Cand.Reason = RegCritical;
    return true;
  }

  unsigned TryOp = TryCand.SU->getInstr()->getOpcode();
  bool TryIsLoad = TryOp == AMDGPU::TENSOR_LOAD_TO_LDS_D2 || TryOp == AMDGPU::S_WAIT_TENSORCNT || TryOp == AMDGPU::S_BARRIER_WAIT || TryOp == AMDGPU::S_BARRIER_SIGNAL_IMM;
  if (TryIsLoad) {
    TryCand.Reason = RegCritical;
    return true;
  }

  unsigned Cutoff = std::min(HWUInfo.size(), (size_t)ResourcesToBalance);
  unsigned CheckedResources = 0;
  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HardwareUnitInfo HWUI = HWUInfo[I];
    if (CheckedResources++ >= Cutoff)
      return false;
  
    unsigned MaxAvailableLat = Zone->findMaxLatency(Zone->Available.elements());
    unsigned CriticalUsage = HWUI.getTotalCycles();

    if (MaxAvailableLat > CriticalUsage)
      return false;

    bool CandUsesCrit = HWUI.contains(Cand.SU);
    bool TryCandUsesCrit = HWUI.contains(TryCand.SU);

    if (!CandUsesCrit && !TryCandUsesCrit)
      continue;

    if (CandUsesCrit && !TryCandUsesCrit) {
      if (Cand.Reason > RegCritical)
        Cand.Reason = RegCritical;
      return true;
    }

    if (!CandUsesCrit && TryCandUsesCrit) {
      TryCand.Reason = RegCritical;
      return true;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone)) {
      return true;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true)) {
      return true;
    }

    if (HWUI.isHigherPriority(Cand.SU, TryCand.SU)) {
      if (Cand.Reason > RegCritical)
        Cand.Reason = RegCritical;
      return true;
    }

    TryCand.Reason = RegCritical;
    return true;
  }

  return false;
}

bool AMDGPUMLSchedStrategy::tryCriticalResourceDependency(
    SchedCandidate &TryCand, SchedCandidate &Cand, SchedBoundary *Zone, 
    bool IsAsync) const {

  auto IsCandidateResource = [Zone, this](unsigned ResourceIdx) {
    unsigned MaxAvailableLat = Zone->findMaxLatency(Zone->Available.elements());
    HardwareUnitInfo HWUI = HWUInfo[ResourceIdx];
    unsigned CriticalUsage = HWUI.getTotalCycles();

    if (MaxAvailableLat > CriticalUsage)
      return false;

    auto *TargetSU = HWUI.getNextTargetSU();
    if (!TargetSU)
      return false;

    return true;
  };

  auto TryEnablesResource = [&Cand, &TryCand, this](unsigned ResourceIdx) {
    HardwareUnitInfo HWUI = HWUInfo[ResourceIdx];
    auto *TargetSU = HWUI.getNextTargetSU();

    bool CandEnables =
        TargetSU != Cand.SU && DAG->IsReachable(TargetSU, Cand.SU);
    bool TryCandEnables =
        TargetSU != TryCand.SU && DAG->IsReachable(TargetSU, TryCand.SU);

    if (!CandEnables && !TryCandEnables)
      return false;

    if (CandEnables && !TryCandEnables) {
      if (Cand.Reason > RegCritical)
        Cand.Reason = RegCritical;

      return true;
    }

    if (!CandEnables && TryCandEnables) {
      TryCand.Reason = RegCritical;
      return true;
    }

    // Both enable, prefer the critical path.
    bool CandHeight = Cand.SU->getHeight();
    bool TryCandHeight = TryCand.SU->getHeight();

    if (CandHeight > TryCandHeight) {
      if (Cand.Reason > RegCritical)
        Cand.Reason = RegCritical;

      return true;
    }

    if (CandHeight < TryCandHeight) {
      TryCand.Reason = RegCritical;
      return true;
    }

    // Same critical path, just prefer original candidate.
    if (Cand.Reason > RegCritical)
      Cand.Reason = RegCritical;

    return true;
  };

  if (IsAsync) {
    for (unsigned I = 0; I < HWUInfo.size(); I++) {
      if (!HWUInfo[I].IsAsync)
        continue;

      if (!IsCandidateResource(I))
        return false;

      return TryEnablesResource(I);
    }
    return false;
  }
   
  unsigned Cutoff = std::min(HWUInfo.size(), (size_t)ResourcesToBalance);
  unsigned CheckedResources = 0;


  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    if (CheckedResources++ >= Cutoff)
      return false;

    // If we have encountered a resource that is not critical, then neither
    // candidate enables a critical resource
    if (!IsCandidateResource(I))
      return false;
    
    bool Enabled = TryEnablesResource(I);
    // If neither has enabled the resource, continue to the next resource
    if (Enabled)
      return true;
  }
  return false;
}

unsigned
AMDGPUMLSchedStrategy::getLatencyStallCycles(SUnit *SU,
                                             unsigned CurrCycle) const {
  unsigned ReadyCycle = SU->TopReadyCycle;
  auto *MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  if (SII->isDS(*MI) && MI->mayLoad()) {
    if (SchedDSR.size() >= 16) {
      unsigned TopOfFIFO = SchedDSR.size() - 16;
      unsigned TopOfFIFOIssue = SchedDSR[TopOfFIFO]->TopReadyCycle;
      // TODO -- should be release at cycle.
      ReadyCycle = std::max(TopOfFIFOIssue + DSLatency, ReadyCycle);
    }
  }

  else if (SII->isMFMAorWMMA(*MI) && SchedMFMA.size()) {
    auto PrevMFMA = SchedMFMA[SchedMFMA.size() - 1];
    unsigned PrevMFMAIssue = PrevMFMA->TopReadyCycle;
    ReadyCycle = std::max(PrevMFMAIssue + PrevMFMA->Latency, ReadyCycle);
  }

  else if (MI->getOpcode() == AMDGPU::TENSOR_LOAD_TO_LDS_D2) {
    return 0;
  }

  else if (SII->isTRANS(*MI) && SchedEXP.size()) {
    auto PrevExp = SchedEXP[SchedEXP.size() - 1];
    unsigned PrevExpIssue = PrevExp->TopReadyCycle;
    ReadyCycle = std::max(PrevExpIssue + 2, ReadyCycle);
  }

  if ((SII->isTRANS(*MI) || (SII->isVALU(*MI) && !MI->mayLoad())) && SchedMFMA.size()) {
    auto PrevMFMA = SchedMFMA[SchedMFMA.size() - 1];
    if (PrevMFMA->Latency == 8) {
      unsigned PrevMFMAIssue = PrevMFMA->TopReadyCycle;
      unsigned CycleDiff = CurrCycle - PrevMFMAIssue;
      if (CycleDiff < 10) {
        if (CycleDiff <= 3) {
          ReadyCycle = std::max(CurrCycle + 3, ReadyCycle);
        }
        else if (CycleDiff <= 6) {
          ReadyCycle = std::max(CurrCycle + 6, ReadyCycle);
        }
        else if (CycleDiff <= 7) {
          ReadyCycle = std::max(CurrCycle + 7, ReadyCycle);
        }
        else {
          ReadyCycle = std::max(CurrCycle + 10, ReadyCycle);
        }
      }
    }
  }

  if (ReadyCycle > CurrCycle) {
    SU->TopReadyCycle = ReadyCycle;
    return ReadyCycle - CurrCycle;
  }
  return 0;
}

bool AMDGPUMLSchedStrategy::tryPendingCandidate(SchedCandidate &Cand,
                                                SchedCandidate &TryCand,
                                                SchedBoundary *Zone) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = NodeOrder;
    return true;
  }

  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg))
    return TryCand.Reason != NoCand;

  // Avoid exceeding the target's limit.
  /*if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.Excess, Cand.RPDelta.Excess, TryCand, Cand,
                  RegExcess, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;*/

  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle()),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle()), TryCand,
                Cand, Stall))
      return TryCand.Reason != NoCand;

    sortResources(HWUInfo);
    if (tryCriticalResource(TryCand, Cand, Zone)) {
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone)) {
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true)) {
      return TryCand.Reason != NoCand;
    }
  }

  return false;
}

bool AMDGPUMLSchedStrategy::tryCandidateBalanced(SchedCandidate &Cand,
                                                 SchedCandidate &TryCand,
                                                 SchedBoundary *Zone) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }

  auto Cycle = CI.getCycle(TryCand.SU->getInstr()->getParent());
  bool InCycle = true;
  if (!Cycle)
    InCycle = false;

  if (!InCycle) {
    // Fall through to original instruction order.
    bool CandIsBArrierSignal = Cand.SU->getInstr()->getOpcode() == AMDGPU::ATOMIC_FENCE;
    if (CandIsBArrierSignal) {
      TryCand.Reason = RegCritical;
      return true;
    }


    
    bool TryCandIsBArrierSignal = TryCand.SU->getInstr()->getOpcode() == AMDGPU::ATOMIC_FENCE;
    if (TryCandIsBArrierSignal) {
      Cand.Reason = RegCritical;
      return true;
    }


    if ((CandIsBArrierSignal || TryCandIsBArrierSignal) && SchedTDM.size()) {
      auto Prev = SchedTDM[SchedTDM.size() - 1];
      auto PrevOp = Prev->getInstr()->getOpcode();
      if (PrevOp == AMDGPU::ATOMIC_FENCE) {
        if (CandIsBArrierSignal) {
          Cand.Reason = RegCritical;
          return true;
        }
        TryCand.Reason = RegCritical;
        return true;
      }

      unsigned CurrCycle = Zone->getCurrCycle();
      if (CandIsBArrierSignal) {
        unsigned ReadyCycle = Cand.SU->TopReadyCycle;
        if (CurrCycle - ReadyCycle >= 100) {
          Cand.Reason = RegCritical;
          return true;
        }
        TryCand.Reason = RegCritical;
        return true;
      }
      if (TryCandIsBArrierSignal) {
        unsigned ReadyCycle = TryCand.SU->TopReadyCycle;
        if (CurrCycle -  ReadyCycle >= 100) {
          TryCand.Reason = RegCritical;
          return true;
        }
        Cand.Reason = RegCritical;
        return true;
      }


    }


    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
      TryCand.Reason = NodeOrder;
      return true;
    }
    return false;
  }


  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg))
    return TryCand.Reason != NoCand;

  // Avoid exceeding the target's limit.
  /*
  if (DAG->isTrackingPressure() && tryPressure(TryCand.RPDelta.Excess,
                                               Cand.RPDelta.Excess,
                                               TryCand, Cand, RegExcess, TRI,
                                               DAG->MF))
    return TryCand.Reason != NoCand;

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() && tryPressure(TryCand.RPDelta.CriticalMax,
                                               Cand.RPDelta.CriticalMax,
                                               TryCand, Cand, RegCritical, TRI,
                                               DAG->MF))
    return TryCand.Reason != NoCand;
*/
  // We only compare a subset of features when comparing nodes between
  // Top and Bottom boundary. Some properties are simply incomparable, in many
  // other instances we should only override the other boundary if something
  // is a clear good pick on one boundary. Skip heuristics that are more
  // "tie-breaking" in nature.
  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {

    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle()),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle()), TryCand,
                Cand, Stall))
      return TryCand.Reason != NoCand;

    sortResources(HWUInfo);

    if (tryCriticalResource(TryCand, Cand, Zone)) {
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone)) {
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true)) {
      return TryCand.Reason != NoCand;
    }

    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
        tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

  }

  // Keep clustered nodes together to encourage downstream peephole
  // optimizations which may reduce resource requirements.
  //
  // This is a best effort to set things up for a post-RA pass. Optimizations
  // like generating loads of multiple registers should ideally be done within
  // the scheduler pass by combining the loads during DAG postprocessing.
  unsigned CandZoneCluster = getClusterID(Cand.AtTop);
  unsigned TryCandZoneCluster = getClusterID(TryCand.AtTop);
  bool CandIsClusterSucc =
      isTheSameCluster(CandZoneCluster, Cand.SU->ParentClusterIdx);
  bool TryCandIsClusterSucc =
      isTheSameCluster(TryCandZoneCluster, TryCand.SU->ParentClusterIdx);

  if (tryGreater(TryCandIsClusterSucc, CandIsClusterSucc, TryCand, Cand,
                 Cluster))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak))
      return TryCand.Reason != NoCand;
  }

  // Avoid increasing the max pressure of the entire region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
                  Cand, RegMax, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Avoid critical resource consumption and balance the schedule.
    TryCand.initResourceDelta(DAG, SchedModel);
    if (tryLess(TryCand.ResDelta.CritResources, Cand.ResDelta.CritResources,
                TryCand, Cand, ResourceReduce)) {
      return TryCand.Reason != NoCand;
    }
    if (tryGreater(TryCand.ResDelta.DemandedResources,
                   Cand.ResDelta.DemandedResources, TryCand, Cand,
                   ResourceDemand)) {
      return TryCand.Reason != NoCand;
    }

    // Avoid serializing long latency dependence chains.
    // For acyclic path limited loops, latency was already checked above.
    if (!RegionPolicy.DisableLatencyHeuristic && TryCand.Policy.ReduceLatency &&
        !Rem.IsAcyclicLatencyLimited && tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
      TryCand.Reason = NodeOrder;
      return true;
    }
  }

  return false;
}

void AMDGPUMLSchedStrategy::pickNodeFromQueue(
    SchedBoundary &Zone, const CandPolicy &ZonePolicy,
    const RegPressureTracker &RPTracker, SchedCandidate &Cand, bool &IsPending,
    bool IsBottomUp) {
  const SIRegisterInfo *SRI = static_cast<const SIRegisterInfo *>(TRI);
  ArrayRef<unsigned> Pressure = RPTracker.getRegSetPressureAtPos();
  unsigned SGPRPressure = 0;
  unsigned VGPRPressure = 0;
  IsPending = false;
  if (DAG->isTrackingPressure()) {
    if (!UseGCNTrackers) {
      SGPRPressure = Pressure[AMDGPU::RegisterPressureSets::SReg_32];
      VGPRPressure = Pressure[AMDGPU::RegisterPressureSets::VGPR_32];
    } else {
      GCNRPTracker *T = IsBottomUp
                            ? static_cast<GCNRPTracker *>(&UpwardTracker)
                            : static_cast<GCNRPTracker *>(&DownwardTracker);
      SGPRPressure = T->getPressure().getSGPRNum();
      VGPRPressure = T->getPressure().getArchVGPRNum();
    }
  }
  LLVM_DEBUG(dbgs() << "Available Q:\n");
  ReadyQueue &AQ = Zone.Available;
  for (SUnit *SU : AQ) {
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    tryCandidateBalanced(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      // Initialize resource delta if needed in case future heuristics query it.
      if (TryCand.ResDelta == SchedResourceDelta())
        TryCand.initResourceDelta(Zone.DAG, SchedModel);
      printCandidateDecision(Cand, TryCand);
      Cand.setBest(TryCand);
    } else {
      printCandidateDecision(TryCand, Cand);
    }
  }

  if (!shouldCheckPending(Zone, SchedModel))
    return;

  LLVM_DEBUG(dbgs() << "Pending Q:\n");
  ReadyQueue &PQ = Zone.Pending;
  for (SUnit *SU : PQ) {
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    AMDGPUMLSchedStrategy::tryPendingCandidate(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      // Initialize resource delta if needed in case future heuristics query it.
      if (TryCand.ResDelta == SchedResourceDelta())
        TryCand.initResourceDelta(Zone.DAG, SchedModel);
      printCandidateDecision(Cand, TryCand);
      IsPending = true;
      Cand.setBest(TryCand);
    } else {
      printCandidateDecision(TryCand, Cand);
    }
  }
}

SUnit *AMDGPUMLSchedStrategy::pickNode(bool &IsTopNode) {
  if (!CollectedUse)
    collectUse();

  if (DAG->top() == DAG->bottom()) {
    assert(Top.Available.empty() && Top.Pending.empty() &&
           Bot.Available.empty() && Bot.Pending.empty() && "ReadyQ garbage");
    return nullptr;
  }
  bool PickedPending;
  SUnit *SU;
  do {
    PickedPending = false;
    if (RegionPolicy.OnlyTopDown) {
      SU = pickOnlyChoice(Top, SchedModel);
      if (!SU) {
        CandPolicy NoPolicy;
        TopCand.reset(NoPolicy);
        pickNodeFromQueue(Top, TopCand.Policy, DAG->getTopRPTracker(), TopCand,
                          PickedPending,
                          /*IsBottomUp=*/false);
        assert(TopCand.Reason != NoCand && "failed to find a candidate");
        SU = TopCand.SU;
      }
      IsTopNode = true;
    } else if (RegionPolicy.OnlyBottomUp) {
      SU = pickOnlyChoice(Bot, SchedModel);
      if (!SU) {
        CandPolicy NoPolicy;
        BotCand.reset(NoPolicy);
        pickNodeFromQueue(Bot, BotCand.Policy, DAG->getBotRPTracker(), BotCand,
                          PickedPending,
                          /*IsBottomUp=*/true);
        assert(BotCand.Reason != NoCand && "failed to find a candidate");
        SU = BotCand.SU;
      }
      IsTopNode = false;
    } else {
      SU = pickNodeBidirectional(IsTopNode, PickedPending);
    }
  } while (SU->isScheduled);

  if (PickedPending) {
    unsigned ReadyCycle = IsTopNode ? SU->TopReadyCycle : SU->BotReadyCycle;
    SchedBoundary &Zone = IsTopNode ? Top : Bot;
    unsigned CurrentCycle = Zone.getCurrCycle();
    if (ReadyCycle > CurrentCycle)
      Zone.bumpCycle(ReadyCycle);

    // FIXME: checkHazard() doesn't give information about which cycle the
    // hazard will resolve so just keep bumping the cycle by 1. This could be
    // made more efficient if checkHazard() returned more details.
    while (Zone.checkHazard(SU))
      Zone.bumpCycle(Zone.getCurrCycle() + 1);

    Zone.releasePending();
  }

  if (SU->isTopReady())
    Top.removeReady(SU);
  if (SU->isBottomReady())
    Bot.removeReady(SU);

  return SU;
}

AMDGPUMLPostSchedStrategy::AMDGPUMLPostSchedStrategy(
    const MachineSchedContext *C)
    : PostGenericScheduler(C) {}

bool AMDGPUMLPostSchedStrategy::tryCandidate(SchedCandidate &Cand,
                                             SchedCandidate &TryCand) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }


  #if 0
  // Prefer WMMA if there is no hazard.
  if (Cand.SU && Cand.SU->getInstr() && TryCand.SU &&
      TryCand.SU->getInstr()) {
    const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);
    bool CandIsWMMA = SII->isMFMAorWMMA(*Cand.SU->getInstr());
    bool TryCandIsWMMA = SII->isMFMAorWMMA(*TryCand.SU->getInstr());

    if (CandIsWMMA != TryCandIsWMMA) {
      if (TryCandIsWMMA) {
        TryCand.Reason = ResourceDemand;
        return true;
      }

      Cand.Reason = ResourceDemand;
      return false;
    }
  }
  # endif

  // Fall through to original instruction order.
  if (TryCand.SU->NodeNum < Cand.SU->NodeNum) {
    TryCand.Reason = NodeOrder;
    return true;
  }

  return false;
}
