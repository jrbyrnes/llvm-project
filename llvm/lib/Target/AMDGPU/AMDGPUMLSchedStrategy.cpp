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
#include "GCNHazardRecognizer.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "machine-scheduler"

using namespace llvm;

static cl::opt<unsigned> ResourcesToBalance(
    "amdgpu-resource-balancing", cl::Hidden,
    cl::desc("Number of resources we will try to balance during scheduling."),
    cl::init(100));

static cl::opt<unsigned> DSLatency(
    "amdgpu-ds-latency", cl::Hidden,
    cl::desc("Latency of DS_LOAD for resource usage."),
    cl::init(55));

static cl::opt<unsigned>
    DSLatencySplit("amdgpu-ds-latency-split", cl::Hidden,
                   cl::desc("Latency between neighboring DS_LOAD."),
                   cl::init(8));

static cl::opt<unsigned>
    DSLatencyFIFO("amdgpu-ds-fifo-latency", cl::Hidden,
                  cl::desc("Hazard latency DS_LOAD FIFO full."), cl::init(55));

static cl::opt<unsigned> LatencyForSignal(
    "amdgpu-signal-latency", cl::Hidden,
    cl::desc("Hazard latency between BARRIER_SIGNAL and BARRIER_WAIT."),
    cl::init(35));

static cl::opt<unsigned> DSLatencyForFence(
    "amdgpu-ds-fence-latency", cl::Hidden,
    cl::desc("Hazard latency between DS_LOAD and FENCE."),
    cl::init(55));

static cl::opt<unsigned> DSFIFOSize("amdgpu-ds-fifo-size", cl::Hidden,
                                    cl::desc("DS_LOAD FIFO size."),
                                    cl::init(8));

bool PreRALog = false;
bool PostRALog = false;

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
  // ML scheduling strategy is only done top-down to support new resource
  // balancing heuristics.
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
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
  
  if (Top.HazardRec) {
    delete Top.HazardRec;
    Top.HazardRec = nullptr;
  }
  Top.HazardRec = new GCNHazardRecognizer(
      DAG->MF, GCNHazardRecognizer::OperatingMode::PreRA);
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

unsigned AMDGPUMLSchedStrategy::getHWUICyclesForInst(SUnit *SU, const SIInstrInfo *SII, unsigned ReleaseAtCycle) {
  auto Opc = SU->getInstr()->getOpcode();
  bool IsDMA = Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2 ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2_gfx1250 ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS ||
                   Opc == AMDGPU::TENSOR_LOAD_TO_LDS_gfx1250 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_gfx1250 ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR ||
                   Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR_gfx1250;
  unsigned Latency = IsDMA ? SU->Latency : ReleaseAtCycle;
  if (SII->isDS(*SU->getInstr()) && SU->getInstr()->mayLoad())
        Latency = DSLatency;

  return Latency;
}

void AMDGPUMLSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  auto MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  // errs() << "SchedNode: "; SU->getInstr()->dump();
  // errs() << "\n\n";
   if (PreRALog) { errs() << "Scheduling: "; MI->dump();}
   if (PreRALog) { errs() << "\n\n";}
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

      unsigned Latency = getHWUICyclesForInst(SU, SII, PI->ReleaseAtCycle);
      HWUInfo[I].schedule(SU, Latency);
    }

    if (SII->isMFMAorWMMA(*MI)) {
      SchedMFMA.push_back(SU);
    }
    if (SII->isDS(*MI) && MI->mayLoad()) {
      SchedDSR.push_back(SU);
    }

    auto Opc = MI->getOpcode();
    if (Opc == AMDGPU::ATOMIC_FENCE || Opc == AMDGPU::S_WAIT_ASYNCCNT || Opc == AMDGPU::S_WAIT_TENSORCNT || Opc == AMDGPU::S_BARRIER_WAIT || Opc == AMDGPU::S_BARRIER_SIGNAL_IMM) {
      SchedTDM.push_back(SU);
    }

    if (SII->isTRANS(*MI)) {
      SchedEXP.push_back(SU);
    }
  }

  GCNSchedStrategy::schedNode(SU, IsTopNode);
}

void AMDGPUMLSchedStrategy::collectUse() {
  CollectedUse = true;
  SchedDSR.clear();
  SchedMFMA.clear();
  SchedTDM.clear();
  SchedEXP.clear();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  for (auto &HWUI : HWUInfo) {
    HWUI.reset();
  }

  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  unsigned I = 0;
  unsigned PrevDSR = 0;
  unsigned PrevFence = 0;
  unsigned FencedDSRCount = 0;
  for (auto &SU : DAG->SUnits) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(&SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      unsigned Latency = getHWUICyclesForInst(&SU, SII, PI->ReleaseAtCycle);
      HWUInfo[PI->ProcResourceIdx].insert(&SU, Latency);
    }

    auto MI = SU.getInstr();
    if (SII->isDS(*MI) && MI->mayLoad()) {
      PrevDSR = I;
    }
    if (MI->getOpcode() == AMDGPU::ATOMIC_FENCE) {
      if (PrevFence < PrevDSR) {
        ++FencedDSRCount;
      }
      PrevFence = I;
    }
    I++;
  }


  unsigned MaxCycles = 0;
  if (FencedDSRCount) {
    for (auto HWUI : HWUInfo) {
      MaxCycles = std::max(MaxCycles, HWUI.getTotalCycles());
    }

    FencedDSRLatency = MaxCycles / FencedDSRCount;
    FencedDSRLatency = std::max(DSLatency.getValue(), FencedDSRLatency);
  }


  HWUInfo[4].reset();
  if (IgnoreVALU) {
    HWUInfo[7].reset();
    HWUInfo[5].reset();
  }

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

static std::optional<unsigned> getMSBs(const MachineOperand &MO,
                                       const SIRegisterInfo *TRI) {
  if (!MO.isReg())
    return std::nullopt;

  MCRegister Reg = MO.getReg();
  const TargetRegisterClass *RC = TRI->getPhysRegBaseClass(Reg);
  if (!RC || !TRI->isVGPRClass(RC))
    return std::nullopt;

  unsigned Idx = TRI->getHWRegIndex(Reg);
  return Idx >> 8;
}

static unsigned
getLatencyStallCycles(SUnit *SU, unsigned CurrCycle, SchedBoundary *Zone,
                      ScheduleDAGInstrs *DAG, const TargetRegisterInfo *TRI,
                      const SmallVectorImpl<SUnit *> &SchedMFMA,
                      const SmallVectorImpl<SUnit *> &SchedDSR,
                      const SmallVectorImpl<SUnit *> &SchedTDM,
                      const SmallVectorImpl<SUnit *> &SchedEXP, bool IsPostRA) {
  const SIRegisterInfo *SRI = static_cast<const SIRegisterInfo *>(TRI);
  unsigned ReadyCycle = SU->TopReadyCycle;
  auto *MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  if (SII->isDS(*MI) && MI->mayLoad()) {
    if (SchedDSR.size() >= DSFIFOSize) {
      unsigned TopOfFIFO = SchedDSR.size() - DSFIFOSize;
      unsigned TopOfFIFOIssue = SchedDSR[TopOfFIFO]->TopReadyCycle;
      // TODO -- should be release at cycle.
      ReadyCycle = std::max(TopOfFIFOIssue + DSLatencyFIFO, ReadyCycle);
    }
    if (SchedDSR.size()) {
      unsigned LastDSRIssue = SchedDSR[SchedDSR.size() - 1]->TopReadyCycle;
      // TODO -- should be release at cycle.
      ReadyCycle = std::max(LastDSRIssue + DSLatencySplit, ReadyCycle);
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

  else if (MI->getOpcode() == AMDGPU::S_BARRIER_WAIT) {
    auto PrevTDM = SchedTDM[SchedTDM.size() - 1];

    if (PrevTDM->getInstr()->getOpcode() == AMDGPU::S_BARRIER_SIGNAL_IMM) {
      ReadyCycle =
          std::max(ReadyCycle, PrevTDM->TopReadyCycle + LatencyForSignal);
    }

  }

  else if ((MI->getOpcode() == AMDGPU::ATOMIC_FENCE ||
            MI->getOpcode() == AMDGPU::S_WAIT_TENSORCNT)) {
    if (SchedDSR.size()) {
      auto PrevDSR = SchedDSR[SchedDSR.size() - 1];
      ReadyCycle =
          std::max(ReadyCycle, PrevDSR->TopReadyCycle + DSLatencyForFence);
    } else {
      ReadyCycle = std::max(ReadyCycle, DSLatencyForFence.getValue());
    }
  }


  if (IsPostRA) {
    if (SchedMFMA.size() && !SII->isMFMAorWMMA(*MI) && (SII->isVALU(*MI) || SII->isTRANS(*MI))) {
      auto PrevMFMA = SchedMFMA[SchedMFMA.size() - 1];
      unsigned PrevMFMAIssue = PrevMFMA->TopReadyCycle;

      if (PrevMFMAIssue + PrevMFMA->Latency > ReadyCycle) {
        for (auto &MO : MI->operands()) {
          if (!MO.isReg())
            continue;
          if (!MO.getReg().isPhysical())
            continue;
          

          if (!SRI->isVGPR(DAG->MRI, MO.getReg()))
            continue;

          for (auto &OtherMO : PrevMFMA->getInstr()->operands()) {
            if (!OtherMO.isReg())
              continue;
            if (!OtherMO.getReg().isPhysical())
              continue;
            if (!SRI->isVGPR(DAG->MRI, OtherMO.getReg()))
            continue;
            
            if (TRI->regsOverlap(MO.getReg(), OtherMO.getReg())) {
              ReadyCycle = std::max(PrevMFMAIssue + PrevMFMA->Latency, ReadyCycle);
              break;
            }
            
          }
        }

      }

    }

    if (SchedEXP.size() && SII->isVALU(*MI) && !SII->isTRANS(*MI)) {
      auto PrevEXP = SchedEXP[SchedEXP.size() - 1];
      unsigned PrevEXPIssue = PrevEXP->TopReadyCycle;

      if (PrevEXPIssue + 2 > ReadyCycle) {
        for (auto &MO : MI->operands()) {
          if (!MO.isReg())
            continue;
          if (!MO.getReg().isPhysical())
            continue;

          if (!SRI->isVGPR(DAG->MRI, MO.getReg()))
            continue;

          for (auto &OtherMO : PrevEXP->getInstr()->operands()) {
            if (!OtherMO.isReg())
              continue;
            if (!OtherMO.getReg().isPhysical())
              continue;
            if (!SRI->isVGPR(DAG->MRI, OtherMO.getReg()))
              continue;

            if (TRI->regsOverlap(MO.getReg(), OtherMO.getReg())) {
              ReadyCycle = std::max(PrevEXPIssue + 2, ReadyCycle);
              break;
            }
          }
        }
      }
    }

    if (SchedMFMA.size() && !SII->isMFMAorWMMA(*MI) &&
        (SII->isVALU(*MI) || SII->isTRANS(*MI))) {
      auto PrevMFMA = SchedMFMA[SchedMFMA.size() - 1];
      unsigned PrevMFMAIssue = PrevMFMA->TopReadyCycle;

      if (PrevMFMAIssue + PrevMFMA->Latency > ReadyCycle) {
        for (auto &MO : MI->operands()) {
          if (!MO.isReg())
            continue;
          if (!MO.getReg().isPhysical())
            continue;

          if (!SRI->isVGPR(DAG->MRI, MO.getReg()))
            continue;

          for (auto &OtherMO : PrevMFMA->getInstr()->operands()) {
            if (!OtherMO.isReg())
              continue;
            if (!OtherMO.getReg().isPhysical())
              continue;
            if (!SRI->isVGPR(DAG->MRI, OtherMO.getReg()))
              continue;

            if (TRI->regsOverlap(MO.getReg(), OtherMO.getReg())) {
              ReadyCycle =
                  std::max(PrevMFMAIssue + PrevMFMA->Latency, ReadyCycle);
              break;
            }
          }
        }
      }
    }

    if (SchedDSR.size()) {
      const SIRegisterInfo *SRI = static_cast<const SIRegisterInfo *>(TRI);
      auto PrevDSR = SchedDSR[SchedDSR.size() - 1];
      unsigned PrevDSRIssue = PrevDSR->TopReadyCycle;
      if (PrevDSRIssue + 2 > ReadyCycle) {
        std::optional<unsigned> DSRMSB;
        for (auto &MO : PrevDSR->getInstr()->operands()) {
          DSRMSB = getMSBs(MO, SRI);
          if (DSRMSB)
            break;
        }
        std::optional<unsigned> ThisMSB;
        for (auto &MO : MI->operands()) {
          ThisMSB = getMSBs(MO, SRI);
          if (ThisMSB)
            break;
        }

        if (ThisMSB && DSRMSB && ThisMSB.value() != DSRMSB.value()) {
          ReadyCycle = std::max(PrevDSRIssue + 2, ReadyCycle);
        }
      }
    }
  }

  GCNHazardRecognizer *HazardRec = static_cast<GCNHazardRecognizer*>(Zone->HazardRec);
  if (HazardRec) {
    unsigned HazardStates = HazardRec->getHazardWaitStates(MI);
    if (HazardStates + CurrCycle > ReadyCycle) {
      // errs() << "Wait for: "; SU->getInstr()->dump();
      // errs() << HazardStates << "\n";
      return HazardStates;
    }
  }

  if (ReadyCycle > CurrCycle) {
    SU->TopReadyCycle = ReadyCycle;
    auto Wait = ReadyCycle - CurrCycle;
    //errs() << "Wait for: "; SU->getInstr()->dump();
    //errs() << Wait << "\n";
    return Wait;
  }

  return 0;
}



static bool tryVALUCoexecSlot(GenericSchedulerBase::SchedCandidate &TryCand,
                               GenericSchedulerBase::SchedCandidate &Cand,
                               SchedBoundary *Zone, ScheduleDAGInstrs *DAG,
                               const TargetRegisterInfo *TRI,
                               const SmallVectorImpl<SUnit *> &SchedMFMA,
                               const SmallVectorImpl<SUnit *> &SchedDSR,
                               const SmallVectorImpl<SUnit *> &SchedTDM,
                               const SmallVectorImpl<SUnit *> &SchedEXP,
                               bool IsPostRA) {
  GCNHazardRecognizer *HazardRec = static_cast<GCNHazardRecognizer *>(Zone->HazardRec);
  int CoexecSlot = HazardRec->getWMMACoexecSlot(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM, SchedEXP, IsPostRA));

  if (CoexecSlot == -1)
    return false;


  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);
  GCNHazardRecognizer::WMMASlotType CurrentSlot = (GCNHazardRecognizer::WMMASlotType)CoexecSlot;
  MachineInstr *TryMI = TryCand.SU->getInstr();
  MachineInstr *CandMI = Cand.SU->getInstr();

  auto PreferNonTransVALU = [SII](GenericSchedulerBase::SchedCandidate &TryCand,
                                  GenericSchedulerBase::SchedCandidate &Cand) {
    MachineInstr *TryMI = TryCand.SU->getInstr();
    MachineInstr *CandMI = Cand.SU->getInstr();
    // We don't want to issue TRANS or CVT here as they (along with WMMA) will
    // clog the whole VALU unit for multiple cycles
    unsigned TryOp = TryMI->getOpcode();
    unsigned CandOp = CandMI->getOpcode();
    bool TryIsSingleCycleVALU =
        SII->isVALU(*TryMI) && !SII->isMFMAorWMMA(*TryMI) &&
        !SII->isTRANS(*TryMI) &&
        TryOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 &&
        TryOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;
    bool CandIsSingleCycleVALU =
        SII->isVALU(*CandMI) && !SII->isMFMAorWMMA(*CandMI) &&
        !SII->isTRANS(*CandMI) &&
        CandOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 &&
        CandOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;

    if (!TryIsSingleCycleVALU && !CandIsSingleCycleVALU) {
      return false;
    }

    if (CandIsSingleCycleVALU && TryIsSingleCycleVALU) {
      return false;
    }
    if (CandIsSingleCycleVALU)
      if (Cand.Reason > GenericSchedulerBase::RegCritical) {
        Cand.Reason = GenericSchedulerBase::RegCritical;
      }

    if (TryIsSingleCycleVALU) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
    }

    return true;
  };

  switch (CurrentSlot) {
    default:
      return false;
    
    case GCNHazardRecognizer::WMMASlotType::MemCoExec0:
    case GCNHazardRecognizer::WMMASlotType::MemCoExec1:
 {
        //errs() << "Mem0/1\n";
      return false;
      bool TryIsMem = SII->isFLATGlobal(*TryMI) || SII->isDS(*TryMI);
      bool CandIsMem = SII->isFLATGlobal(*CandMI) || SII->isDS(*CandMI);

      if (!TryIsMem && !CandIsMem)
        return false;
      
      if (CandIsMem)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
      
      if (TryIsMem)
        TryCand.Reason = GenericSchedulerBase::RegCritical;
      
      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::MemCoExec2:
    case GCNHazardRecognizer::WMMASlotType::MemCoExec3: {
     // errs() << "Mem2/3\n";

      bool TryIsMem = SII->isFLATGlobal(*TryMI) || SII->isDS(*TryMI);
      bool CandIsMem = SII->isFLATGlobal(*CandMI) || SII->isDS(*CandMI);

      if (!TryIsMem && !CandIsMem)
        return PreferNonTransVALU(TryCand, Cand);

    if (CandIsMem && TryIsMem) {
      return false;

    }

      if (CandIsMem)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;

      if (TryIsMem)
        TryCand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }
    case GCNHazardRecognizer::WMMASlotType::ValuBlocked0: {
      //errs() << "ValuBlocked0\n";
      bool TryIsSALU = SII->isMFMAorWMMA(*TryMI);
      bool CandIsSALU = SII->isMFMAorWMMA(*CandMI);

      if (!TryIsSALU && !CandIsSALU)
        return false;

    if (CandIsSALU && TryIsSALU) {
      return false;

    }

      if (CandIsSALU)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;

      if (TryIsSALU)
        TryCand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::ValuBlocked1: {
      //errs() << "Valublocked1\n";
      bool TryIsWMMA = SII->isMFMAorWMMA(*TryMI);
      bool CandIsWMMA = SII->isMFMAorWMMA(*CandMI);
      
      if (!TryIsWMMA && !CandIsWMMA)
        return false;
  
      if (CandIsWMMA && TryIsWMMA) {
        return false;

    }
      if (CandIsWMMA)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
      
      if (TryIsWMMA)
        TryCand.Reason = GenericSchedulerBase::RegCritical;
      
      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoExec1: {
      //errs() << "Valucoexec1\n";
      return PreferNonTransVALU(TryCand, Cand);
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoExec0: {
      //errs() << "Valucoexec0\n";
      // We prefer 2 cycle TRANS here
      unsigned TryOp = TryMI->getOpcode();
      unsigned CandOp = CandMI->getOpcode();
      bool TryIsSingleCycleVALUOrTrans = SII->isVALU(*TryMI) && !SII->isMFMAorWMMA(*TryMI) && TryOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 && TryOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;
      bool CandIsSingleCycleVALUOrTrans = SII->isVALU(*CandMI) && !SII->isMFMAorWMMA(*CandMI) && CandOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 && CandOp != AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;

      bool TryTRANS = SII->isTRANS(*TryMI);
      bool CandTRANS = SII->isTRANS(*CandMI);


      if (!TryTRANS && !CandTRANS) {
        if (!TryIsSingleCycleVALUOrTrans && !CandIsSingleCycleVALUOrTrans)
          return false;
      
      if (TryIsSingleCycleVALUOrTrans && CandIsSingleCycleVALUOrTrans) {
        return false;

      }

        if (CandIsSingleCycleVALUOrTrans)
          if (Cand.Reason > GenericSchedulerBase::RegCritical)
            Cand.Reason = GenericSchedulerBase::RegCritical;
      
        if (TryIsSingleCycleVALUOrTrans)
          TryCand.Reason = GenericSchedulerBase::RegCritical;
      
        return true;
      }

      if (CandTRANS && TryTRANS) {
        return false;
    }


      if (CandTRANS)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
      
      if (TryTRANS)
        TryCand.Reason = GenericSchedulerBase::RegCritical;
      
      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoExec2: {
      //errs() << "Valucoexec2\n";
      // We don't want to issue TRANS or CVT here as they (along with WMMA) will
      // clog the whole VALU unit for multiple cycles
      unsigned TryOp = TryMI->getOpcode();
      unsigned CandOp = CandMI->getOpcode();
      bool TryIsSingleCycleVALUOrTrans = SII->isVALU(*TryMI) &&
                                         !SII->isMFMAorWMMA(*TryMI) &&
                                         !SII->isTRANS(*TryMI);
      bool CandIsSingleCycleVALUOrTrans = SII->isVALU(*CandMI) &&
                                          !SII->isMFMAorWMMA(*CandMI) &&
                                          !SII->isTRANS(*CandMI);

      bool TryLongLat = TryOp == AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 ||
                        TryOp == AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;
      bool CandLongLat =
          CandOp == AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64 ||
          CandOp == AMDGPU::V_CVT_SCALEF32_PK8_FP8_F32_e64_gfx1250;

      if (!TryLongLat && !CandLongLat) {
        if (!TryIsSingleCycleVALUOrTrans && !CandIsSingleCycleVALUOrTrans)
          return false;

      if (CandIsSingleCycleVALUOrTrans && TryIsSingleCycleVALUOrTrans) {
        return false;

    }

        if (CandIsSingleCycleVALUOrTrans)
          if (Cand.Reason > GenericSchedulerBase::RegCritical)
            Cand.Reason = GenericSchedulerBase::RegCritical;

        if (TryIsSingleCycleVALUOrTrans)
          TryCand.Reason = GenericSchedulerBase::RegCritical;

        return true;
      }

      if (CandLongLat && TryLongLat) {
        return false;
    }

      if (CandLongLat)
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;

      if (TryLongLat)
        TryCand.Reason = GenericSchedulerBase::RegCritical;
      
      return true;
    }
  }

  return false;
}

static bool
tryCriticalResourceDependency(GenericSchedulerBase::SchedCandidate &TryCand,
                               GenericSchedulerBase::SchedCandidate &Cand,
                               SchedBoundary *Zone, bool IsAsync,
                               const SmallVectorImpl<HardwareUnitInfo> &HWUInfo,
                               ScheduleDAGInstrs *DAG, bool IsPostRA) {

  auto IsCandidateResource = [Zone, &HWUInfo](unsigned ResourceIdx) {
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

  auto TryEnablesResource = [&Cand, &TryCand, &HWUInfo,
                             DAG](unsigned ResourceIdx) {
    HardwareUnitInfo HWUI = HWUInfo[ResourceIdx];
    auto *TargetSU = HWUI.getNextTargetSU();

    bool CandEnables =
        TargetSU != Cand.SU && DAG->IsReachable(TargetSU, Cand.SU);
    bool TryCandEnables =
        TargetSU != TryCand.SU && DAG->IsReachable(TargetSU, TryCand.SU);

    if (!CandEnables && !TryCandEnables)
      return false;

    if (CandEnables && !TryCandEnables) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    if (!CandEnables && TryCandEnables) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Both enable, prefer the critical path.
    bool CandHeight = Cand.SU->getHeight();
    bool TryCandHeight = TryCand.SU->getHeight();

    if (CandHeight > TryCandHeight) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    if (CandHeight < TryCandHeight) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Same critical path, just prefer original candidate.
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;

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

static bool tryCriticalResource(GenericSchedulerBase::SchedCandidate &TryCand,
                                 GenericSchedulerBase::SchedCandidate &Cand,
                                 SchedBoundary *Zone,
                                 SmallVectorImpl<HardwareUnitInfo> &HWUInfo,
                                 ScheduleDAGInstrs *DAG, bool IsPostRA) {
  unsigned CandOp = Cand.SU->getInstr()->getOpcode();
  bool CandIsLoad = CandOp == AMDGPU::TENSOR_LOAD_TO_LDS_D2 || CandOp == AMDGPU::S_WAIT_TENSORCNT || CandOp == AMDGPU::S_BARRIER_WAIT || CandOp == AMDGPU::S_BARRIER_SIGNAL_IMM;
  if (CandIsLoad) {
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }

  unsigned TryOp = TryCand.SU->getInstr()->getOpcode();
  bool TryIsLoad = TryOp == AMDGPU::TENSOR_LOAD_TO_LDS_D2 || TryOp == AMDGPU::S_WAIT_TENSORCNT || TryOp == AMDGPU::S_BARRIER_WAIT || TryOp == AMDGPU::S_BARRIER_SIGNAL_IMM;
  if (TryIsLoad) {
    TryCand.Reason = GenericSchedulerBase::RegCritical;
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
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    if (!CandUsesCrit && TryCandUsesCrit) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       IsPostRA)) {
      return true;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       IsPostRA)) {
      return true;
    }

    if (HWUI.isHigherPriority(Cand.SU, TryCand.SU)) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    TryCand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }

  return false;
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
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg)) {
                  if (PreRALog) { errs() << "PhysReg\n"; }
    return TryCand.Reason != NoCand;
                 }

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
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                TryCand, Cand, Stall)) {
      if (PreRALog) {
        errs() << "Stall\n";
      }
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false)) {
      if (PreRALog) { errs() << "ValuCoexec\n";}
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, false)) {
      if (PreRALog) { errs() << "CritResource\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       false)) {
                                        if (PreRALog) { errs() << "CritResourceDep\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       false)) {
                                       if (PreRALog) {errs() << "CritResourceDep Async\n";}
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


  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg)) {
                  if (PreRALog) { errs() << "PhysReg\n";}
    return TryCand.Reason != NoCand;
                 }

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
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                TryCand, Cand, Stall)) {
      if (PreRALog) {
        errs() << "Stall\n";
      }
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false)) {
      if (PreRALog) { errs() << "VALUCoexec\n";}
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo);

    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, false)) {
      if (PreRALog) { errs() << "CritResource\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       false)) {
                                      if (PreRALog) {  errs() << "CritResourceDep\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       false)) {
                                        if (PreRALog) { errs() << "CritResourceDep Async\n";}
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
                 Cluster)) {
                 if (PreRALog) {  errs() << "Cluster\n";}
    return TryCand.Reason != NoCand;
                 }

  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak)) {
                  if (PreRALog) { errs() << "Weak\n";}
      return TryCand.Reason != NoCand;
                }
  }

  // Avoid increasing the max pressure of the entire region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
                  Cand, RegMax, TRI, DAG->MF)) {
                   if (PreRALog) {  errs() << "RP\n";}
    return TryCand.Reason != NoCand;
                  }

  // Fall through to original instruction order.
  if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
      (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
       if (PreRALog) { errs() << "NID\n"; }
    TryCand.Reason = NodeOrder;
    return true;
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
  if (PreRALog) { errs() << "Checking Available: \n";}
  ReadyQueue &AQ = Zone.Available;
  for (SUnit *SU : AQ) {
    if (PreRALog) { SU->getInstr()->dump();} 
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    tryCandidateBalanced(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      if (PreRALog) { errs() << "NewBest!\n"; }
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
  if (PreRALog) { errs() << "Checking Pending:\n";}
  ReadyQueue &PQ = Zone.Pending;
  for (SUnit *SU : PQ) {
    if (PreRALog) { SU->getInstr()->dump();}
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    AMDGPUMLSchedStrategy::tryPendingCandidate(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      if (PreRALog) { errs() << "NewBest!\n";}
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
                                             SchedCandidate &TryCand, SchedBoundary *Zone) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }

  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {

    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true),
                TryCand, Cand, Stall)) {
      if (PostRALog) {
        errs() << "Stall\n";
      }
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true)) {
      if (PostRALog) {errs() << "VALUCoexec\n";}
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo);

    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, true)) {
      if (PostRALog) { errs() << "CritResource\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       true)) {
                                        if (PostRALog) { errs() << "CritResourceDep\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       true)) {
                                        if (PostRALog) { errs() << "CritResourceDepAsync\n";}
      return TryCand.Reason != NoCand;
    }

    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
        tryLatency(TryCand, Cand, *Zone)) {
          if (PostRALog) {errs() << "Latency\n";}
      return TryCand.Reason != NoCand;
        }
  }

  // Keep clustered nodes together to encourage downstream peephole
  // optimizations which may reduce resource requirements.
  //
  // This is a best effort to set things up for a post-RA pass. Optimizations
  // like generating loads of multiple registers should ideally be done within
  // the scheduler pass by combining the loads during DAG postprocessing.
  /*
  unsigned CandZoneCluster = getClusterID(Cand.AtTop);
  unsigned TryCandZoneCluster = getClusterID(TryCand.AtTop);
  bool CandIsClusterSucc =
      isTheSameCluster(CandZoneCluster, Cand.SU->ParentClusterIdx);
  bool TryCandIsClusterSucc =
      isTheSameCluster(TryCandZoneCluster, TryCand.SU->ParentClusterIdx);

  if (tryGreater(TryCandIsClusterSucc, CandIsClusterSucc, TryCand, Cand,
                 Cluster))
    return TryCand.Reason != NoCand;
*/
  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak)) {
                  if (PostRALog) {errs() << "Cluster\n";}
      return TryCand.Reason != NoCand;
                }
  }

  if (SameBoundary) {
    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
          if (PostRALog) {errs() << "NID\n";}
      TryCand.Reason = NodeOrder;
      return true;
    }
  }

  return false;
}

bool AMDGPUMLPostSchedStrategy::tryPendingCandidate(SchedCandidate &Cand,
                                                    SchedCandidate &TryCand,
                                                    SchedBoundary *Zone) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = NodeOrder;
    return true;
  }


  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true),
                TryCand, Cand, Stall)) {
      if (PostRALog) {
        errs() << "Stall\n";
      }
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true)) {
      if (PostRALog) {errs() << "Coexec\n";}
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, true)) {
      if (PostRALog) {errs() << "CritResource\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       true)) {
                                        if (PostRALog) {errs() << "CritResourceDep\n";}
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       true)) {
                                       if (PostRALog) { errs() << "CritResourceDep Async\n";}
      return TryCand.Reason != NoCand;
    }
  }

  if (SameBoundary) {
    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
          if (PostRALog) {errs() << "NID\n";}
      TryCand.Reason = NodeOrder;
      return true;
    }
  }

  return false;
}

void AMDGPUMLPostSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
   if (PostRALog) {errs() << "Scheduling: "; DAG->dumpNode(*SU);}
   if (PostRALog) {errs() << "\n\n";}
  auto MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);



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
    if (Opc == AMDGPU::ATOMIC_FENCE || Opc == AMDGPU::S_WAIT_ASYNCCNT || Opc == AMDGPU::S_WAIT_TENSORCNT || Opc == AMDGPU::S_BARRIER_WAIT || Opc == AMDGPU::S_BARRIER_SIGNAL_IMM) {
      SchedTDM.push_back(SU);
    }

  PostGenericScheduler::schedNode(SU, IsTopNode);
}


void AMDGPUMLPostSchedStrategy::collectUse() {
  // errs() << "PostRA collect use\n";
  CollectedUse = true;
  SchedDSR.clear();
  SchedMFMA.clear();
  SchedTDM.clear();
  SchedEXP.clear();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  for (auto &HWUI : HWUInfo) {
    HWUI.reset();
  }

  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  unsigned I = 0;
  unsigned PrevDSR = 0;
  unsigned PrevFence = 0;
  unsigned FencedDSRCount = 0;
  for (auto &SU : DAG->SUnits) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(&SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      unsigned Latency = getHWUICyclesForInst(&SU, SII, PI->ReleaseAtCycle);
      HWUInfo[PI->ProcResourceIdx].insert(&SU, Latency);
    }

    auto MI = SU.getInstr();
    if (SII->isDS(*MI) && MI->mayLoad()) {
      PrevDSR = I;
    }
    if (MI->getOpcode() == AMDGPU::ATOMIC_FENCE) {
      if (PrevFence < PrevDSR) {
        ++FencedDSRCount;
      }
      PrevFence = I;
    }
    I++;
  }

  // errs() << "\n\nAfter Collect use:\n";
  // for (auto &HWUI : HWUInfo) {
  //   errs() << "HWUI " << HWUI.Idx << ", has: " << HWUI.getTotalCycles() <<
  //   "\n";
  // }

  unsigned MaxCycles = 0;
  if (FencedDSRCount) {
    for (auto HWUI : HWUInfo) {
      MaxCycles = std::max(MaxCycles, HWUI.getTotalCycles());
    }

    FencedDSRLatency = MaxCycles / FencedDSRCount;
    FencedDSRLatency = std::max(DSLatency.getValue(), FencedDSRLatency);
  }

  HWUInfo[4].reset();
  if (IgnoreVALU)
    HWUInfo[7].reset();
}

unsigned AMDGPUMLPostSchedStrategy::getHWUICyclesForInst(
    SUnit *SU, const SIInstrInfo *SII, unsigned ReleaseAtCycle) {
  auto Opc = SU->getInstr()->getOpcode();
  bool IsDMA = Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2 ||
               Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2_gfx1250 ||
               Opc == AMDGPU::TENSOR_LOAD_TO_LDS ||
               Opc == AMDGPU::TENSOR_LOAD_TO_LDS_gfx1250 ||
               Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32 ||
               Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_gfx1250 ||
               Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR ||
               Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR_gfx1250;
  unsigned Latency = IsDMA ? SU->Latency : ReleaseAtCycle;
  if (SII->isDS(*SU->getInstr()) && SU->getInstr()->mayLoad())
    Latency = DSLatency;

  return Latency;
}

void AMDGPUMLPostSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  // ML scheduling strategy is only done top-down to support new resource
  // balancing heuristics.
  if (PostRALog) { errs() << "New region\n";}
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
  PostGenericScheduler::initialize(DAG);

  const MCSchedModel &SM = DAG->MF.getSubtarget().getSchedModel();
  unsigned NumPR = SM.getNumProcResourceKinds();
  HWUInfo.resize(NumPR);
  for (unsigned I = 0; I < NumPR; I++) {
    HWUInfo[I].setRes(SM.getProcResource(I));
    HWUInfo[I].Idx = I;
  }
  if (NumPR > 8)
    HWUInfo[8].IsAsync = true;

  CollectedUse = false;

  SchedDSR.clear();
  SchedMFMA.clear();
  SchedTDM.clear();

  for (auto &HWUI : HWUInfo) {
    HWUI.reset();
  }

  if (Top.HazardRec) {
    delete Top.HazardRec;
    Top.HazardRec = nullptr;
  }
  Top.HazardRec = new GCNHazardRecognizer(
      DAG->MF, GCNHazardRecognizer::OperatingMode::PostRA);
}

void AMDGPUMLPostSchedStrategy::pickNodeFromQueue(SchedBoundary &Zone,
                                                  SchedCandidate &Cand,
                                                  bool &IsPending) {
  ReadyQueue &Q = Zone.Available;
  if (PostRALog) { errs() << "Checking Available\n";}
  for (SUnit *SU : Q) {
    if (PostRALog) { DAG->dumpNode(*SU);}
    SchedCandidate TryCand(Cand.Policy);
    TryCand.SU = SU;
    TryCand.AtTop = Zone.isTop();
    TryCand.initResourceDelta(DAG, SchedModel);
    // errs() << "Trying available SU: "; SU->getInstr()->dump();
    if (AMDGPUMLPostSchedStrategy::tryCandidate(Cand, TryCand, &Zone)) {
      IsPending = false;
      Cand.setBest(TryCand);
       if (PostRALog) { errs() << "NewBest\n";}
      LLVM_DEBUG(traceCandidate(Cand));
    }
  }

  ReadyQueue &PQ = Zone.Pending;
  if (PostRALog) { errs() << "Checking Pending\n";}
  for (SUnit *SU : PQ) {
    if (PostRALog) { DAG->dumpNode(*SU);}
    SchedCandidate TryCand(Cand.Policy);
    TryCand.SU = SU;
    TryCand.AtTop = Zone.isTop();
    TryCand.initResourceDelta(DAG, SchedModel);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    // errs() << "Trying pending SU: "; SU->getInstr()->dump();
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    if (tryPendingCandidate(Cand, TryCand, ZoneArg)) {
      if (PostRALog) { errs() << "NewBest\n";}
      IsPending = true;
      Cand.setBest(TryCand);
      LLVM_DEBUG(traceCandidate(Cand));
      // errs() << "NewBest\n";
    }
  }
}

/// Pick the next node to schedule.
SUnit *AMDGPUMLPostSchedStrategy::pickNode(bool &IsTopNode) {
  if (!CollectedUse)
    collectUse();
  bool IsPending = false;
  if (DAG->top() == DAG->bottom()) {
    assert(Top.Available.empty() && Top.Pending.empty() &&
           Bot.Available.empty() && Bot.Pending.empty() && "ReadyQ garbage");
    return nullptr;
  }
  SUnit *SU;
  if (RegionPolicy.OnlyBottomUp) {
    SU = pickOnlyChoice(Top, SchedModel);
    if (!SU) {
      CandPolicy NoPolicy;
      BotCand.reset(NoPolicy);
      // Set the bottom-up policy based on the state of the current bottom
      // zone and the instructions outside the zone, including the top zone.
      setPolicy(BotCand.Policy, /*IsPostRA=*/true, Bot, nullptr);
      pickNodeFromQueue(Bot, BotCand, IsPending);
      assert(BotCand.Reason != NoCand && "failed to find a candidate");
      SU = BotCand.SU;
    }
    IsTopNode = false;
  } else if (RegionPolicy.OnlyTopDown) {
    SU = pickOnlyChoice(Top, SchedModel);
    if (!SU) {
      // errs() << "PostRA TOpdown\n";
      CandPolicy NoPolicy;
      TopCand.reset(NoPolicy);
      // Set the top-down policy based on the state of the current top zone
      // and the instructions outside the zone, including the bottom zone.
      setPolicy(TopCand.Policy, /*IsPostRA=*/true, Top, nullptr);
      pickNodeFromQueue(Top, TopCand, IsPending);
      assert(TopCand.Reason != NoCand && "failed to find a candidate");

      SU = TopCand.SU;
    }
    IsTopNode = true;

  } else {
    SU = pickNodeBidirectional(IsTopNode, IsPending);
  }
  assert(!SU->isScheduled && "SUnit scheduled twice.");

  if (IsPending) {
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

  LLVM_DEBUG(dbgs() << "Scheduling SU(" << SU->NodeNum << ") "
                    << *SU->getInstr());

  return SU;
}