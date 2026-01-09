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
    cl::init(0));

static cl::opt<unsigned>
    DSLatency("amdgpu-ds-latency", cl::Hidden,
              cl::desc("Latency of DS_LOAD for resource usage."), cl::init(44));

static cl::opt<unsigned>
    DSLatencySplit("amdgpu-ds-latency-split", cl::Hidden,
                   cl::desc("Latency between neighboring DS_LOAD."),
                   cl::init(4));

static cl::opt<unsigned>
    DSLatencyFIFO("amdgpu-ds-fifo-latency", cl::Hidden,
                  cl::desc("Hazard latency DS_LOAD FIFO full."), cl::init(98));

static cl::opt<unsigned> LatencyForSignal(
    "amdgpu-signal-latency", cl::Hidden,
    cl::desc("Hazard latency between BARRIER_SIGNAL and BARRIER_WAIT."),
    cl::init(4));

static cl::opt<unsigned>
    DSLatencyForFence("amdgpu-ds-fence-latency", cl::Hidden,
                      cl::desc("Hazard latency between DS_LOAD and FENCE."),
                      cl::init(73));

static cl::opt<unsigned> DSFIFOSize("amdgpu-ds-fifo-size", cl::Hidden,
                                    cl::desc("DS_LOAD FIFO size."),
                                    cl::init(16));

static cl::opt<bool> IgnoreVALU(
    "amdgpu-ignore-valu-resource-balancing", cl::Hidden,
    cl::desc(
        "Whether or not to ignore VALU unit when balancing HW resoiurces."),
    cl::init(false));

static cl::opt<bool> AvoidEXP(
  "amdgpu-avoid-exp-final-islot", cl::Hidden,
  cl::desc("Whether or not to try avoiding putting v_exp in final I slot of WMMA."),
  cl::init(false));

static cl::opt<bool> EnableShadowMix(
  "amdgpu-shadow-mix", cl::Hidden,
  cl::desc("Enable shadow mix lookahead scheduling to ensure co-execution "
           "opportunities (e.g., WMMA with VALU/DS) are available before "
           "scheduling long-latency instructions."),
  cl::init(true));

static cl::opt<unsigned> ShadowMixWMMAMinVALU1c(
    "amdgpu-shadow-mix-wmma-min-valu1c", cl::Hidden,
    cl::desc("Minimum number of ready single-cycle VALU instructions required "
             "before scheduling a WMMA instruction. Setting to 0 disables "
             "VALU check. WMMA has 8 co-execution slots that can be filled "
             "with 1-cycle VALU, so 2 ensures interleaving opportunity."),
    cl::init(6));

static cl::opt<unsigned> ShadowMixWMMAMinDS(
    "amdgpu-shadow-mix-wmma-min-ds", cl::Hidden,
    cl::desc(
        "Minimum number of ready DS (LDS load/store) instructions required "
        "before scheduling a WMMA instruction. Setting to 0 disables "
        "DS check. WMMA's first co-exec slot can accommodate a DS_LOAD."),
    cl::init(6));

static cl::opt<unsigned> ShadowMixWMMAMinSALU(
  "amdgpu-shadow-mix-wmma-min-salu", cl::Hidden,
  cl::desc("Minimum number of ready SALU instructions required "
           "before scheduling a WMMA instruction. Setting to 0 disables "
           "SALU check. SALU can fill WMMA co-exec slots."),
  cl::init(0));

static cl::opt<unsigned> ShadowMixLookaheadDepth(
    "amdgpu-shadow-mix-lookahead-depth", cl::Hidden,
    cl::desc("Maximum dependency depth to search when looking for pending "
             "co-execution candidates. Higher values find more opportunities "
             "but increase compile time. 0 disables lookahead (direct enable "
             "only)."),
    cl::init(15));

static cl::opt<unsigned> ShadowMixMaxBlockingCost(
    "amdgpu-shadow-mix-max-blocking-cost", cl::Hidden,
    cl::desc(
        "Maximum number of blocking instructions acceptable when searching "
        "for pending co-execution candidates. Targets with higher cost are "
        "ignored as too expensive to reach."),
    cl::init(7));

static cl::opt<unsigned> ShadowMixMaxVisited(
  "amdgpu-shadow-mix-max-visited", cl::Hidden,
  cl::desc("Maximum number of nodes to visit during blocking count BFS. "
           "Limits compile time for large DAGs."),
  cl::init(1000));

static cl::opt<unsigned> ShadowMixMaxCandidates(
  "amdgpu-shadow-mix-max-candidates", cl::Hidden,
  cl::desc("Maximum number of pending candidates to examine during lookahead. "
           "Limits compile time when many pending instructions exist."),
  cl::init(5));

// Shadow priority rules: prefer long-latency instruction so short ones fill shadow.
// These are toggleable for debugging the increasingly specific heuristics.
static cl::opt<bool> ShadowPriorityWMMAOverDS(
    "amdgpu-shadow-priority-wmma-over-ds", cl::Hidden,
    cl::desc("Prefer WMMA over DS when both ready (DS fills WMMA shadow)."),
    cl::init(false));

static cl::opt<bool> ShadowPriorityWMMAOverSALU(
    "amdgpu-shadow-priority-wmma-over-salu", cl::Hidden,
    cl::desc("Prefer WMMA over SALU when both ready (SALU fills WMMA shadow)."),
    cl::init(true));

static cl::opt<bool> ShadowPriorityCVTOverDS(
  "amdgpu-shadow-priority-cvt-over-ds", cl::Hidden,
  cl::desc("Prefer CVT over DS when both ready (DS fills CVT shadow)."),
  cl::init(true));

static cl::opt<bool> ShadowPriorityCVTOverSALU(
  "amdgpu-shadow-priority-cvt-over-salu", cl::Hidden,
  cl::desc("Prefer CVT over SALU when both ready (SALU fills CVT shadow)."),
  cl::init(true));

static cl::opt<bool> ShadowPriorityTRANS32OverVALU1c(
    "amdgpu-shadow-priority-trans32-over-valu1c", cl::Hidden,
    cl::desc("Prefer TRANS32 (v_exp etc) over 1-cycle VALU when both ready "
             "(VALU fills TRANS32 shadow)."),
    cl::init(true));

static cl::opt<bool> ShadowDeferTRANS32(
  "amdgpu-shadow-defer-trans32", cl::Hidden,
  cl::desc("Defer TRANS32 instructions until enough VALU ready to fill shadow."),
  cl::init(true));

static cl::opt<unsigned> ShadowMixTRANS32MinVALU1c(
    "amdgpu-shadow-mix-trans32-min-valu1c", cl::Hidden,
    cl::desc(
        "Minimum 1-cycle VALU instructions ready before scheduling TRANS32 "
        "(when -amdgpu-shadow-defer-trans32 enabled)."),
    cl::init(3));

static cl::opt<bool> ShadowPreferVALU1cOverSALUForTRANS(
  "amdgpu-shadow-prefer-valu-over-salu-for-trans", cl::Hidden,
  cl::desc("When filling TRANS32 shadow, prefer VALU1c over SALU "
           "(reserve SALU for WMMA/CVT shadows)."),
  cl::init(false));

// Flag seems universally beneficial, may make sense to delete
static cl::opt<bool> ResourcePriorityToProducer(
    "amdgpu-resource-priority-coexec-producer", cl::Hidden,
    cl::desc("When sorting critical resources, whether to give more priortiy "
             "to coexecution producers over exposed latency."),
    cl::init(true));

// Flag seems universally beneficial, may make sense to delete
static cl::opt<bool> ResourcePriorityCoexecWindowSize(
    "amdgpu-resource-priority-coexec-windows-size", cl::Hidden,
    cl::desc(
        "When sorting critical resources, whether to sort by window size."),
    cl::init(true));

// Flag seems universally beneficial, may make sense to delete
static cl::opt<bool> ResourcePriorityExposedCycles(
    "amdgpu-resource-priority-coexec-exposed-cycles", cl::Hidden,
    cl::desc(
        "When sorting critical resources, whether to sort by exposed cycles."),
    cl::init(true));

static cl::opt<bool> ShadowMixRules(
    "amdgpu-use-shadow-mix-rules", cl::Hidden,
    cl::desc("Whether to use instruction type rules in tryShadowMix."),
    cl::init(false));


static cl::opt<unsigned> ShadowMixWMMAMinWMMA(
  "amdgpu-shadow-mix-wmma-min-wmma", cl::Hidden,
  cl::desc("Minimum number of ready WMMA instructions required "
           "before attempting to free up coexecution instructionsn. Setting to 0 disables "
           "WMMA check."),
  cl::init(2));

//===----------------------------------------------------------------------===//
// Shadow Mix Lookahead Helpers
//===----------------------------------------------------------------------===//

/// Count how many successors of SU would become ready (NumPredsLeft == 1)\n/// and match the target flavor.
// FIXME - should we adjust cost for non-hideable instructions as determined by collectUse?
static unsigned countDirectlyEnabledByFlavor(SUnit *SU, InstructionFlavor TargetF,
                                              const SIInstrInfo *SII) {
  unsigned Count = 0;
  for (const SDep &Succ : SU->Succs) {
    if (Succ.isWeak())
      continue;
    SUnit *SuccSU = Succ.getSUnit();
    if (SuccSU->isBoundaryNode() || SuccSU->isScheduled)
      continue;
    // Would become ready if SU is scheduled
    if (SuccSU->NumPredsLeft == 1) {
      InstructionFlavor F = classifyFlavor(SuccSU->getInstr(), SII);
      if (F == TargetF)
        ++Count;
    }
  }
  return Count;
}

/// Compute bounded blocking depth: how many unscheduled instructions
/// transitively block TargetSU, up to MaxDepth levels and MaxVisited nodes.
/// Returns {instruction count, whether search was truncated}.
static std::pair<unsigned, bool>
computeBoundedBlockingCount(SUnit *TargetSU, unsigned MaxDepth, unsigned MaxVisited) {
  if (TargetSU->isTopReady() || MaxDepth == 0)
    return {0, false};

  unsigned Count = 0;
  bool Truncated = false;
  SmallVector<std::pair<SUnit *, unsigned>, 16> WorkList;
  DenseSet<SUnit *> Visited;

  WorkList.push_back({TargetSU, 0});

  while (!WorkList.empty()) {
    auto [SU, Depth] = WorkList.pop_back_val();
    if (Depth >= MaxDepth || Visited.size() >= MaxVisited) {
      Truncated = true;
      continue;
    }
    if (!Visited.insert(SU).second)
      continue;

    for (const SDep &Pred : SU->Preds) {
      if (Pred.isWeak())
        continue;
      SUnit *PredSU = Pred.getSUnit();
      if (PredSU->isBoundaryNode() || PredSU->isScheduled)
        continue;
      ++Count;
      WorkList.push_back({PredSU, Depth + 1});
    }
  }
  return {Count, Truncated};
}

/// Find the nearest pending single-cycle VALU and compute its blocking cost.
/// Returns {SUnit*, blocking count} or {nullptr, UINT_MAX} if none found.
static std::pair<SUnit *, unsigned>
findNearestPendingByFlavor(const RegionMixInfo &MixInfo, InstructionFlavor Flavor,
                           unsigned MaxDepth, unsigned MaxCost,
                           unsigned MaxVisited, unsigned MaxCandidates) {
  SUnit *BestSU = nullptr;
  unsigned BestCost = UINT_MAX;
  unsigned CandidatesChecked = 0;

  for (SUnit *SU : MixInfo.getSUs(Flavor)) {
    if (SU->isScheduled || SU->isTopReady())
      continue;

    if (++CandidatesChecked > MaxCandidates)
      break;

    auto [Cost, Truncated] = computeBoundedBlockingCount(SU, MaxDepth, MaxVisited);
    // Skip if too expensive or search was truncated (likely too deep)
    if (Truncated || Cost > MaxCost)
      continue;

    if (Cost < BestCost) {
      BestCost = Cost;
      BestSU = SU;
    }
  }
  return {BestSU, BestCost};
}

/// Check if scheduling SU would help enable a target SU (is on path to it).
static bool wouldHelpEnable(SUnit *SU, SUnit *TargetSU,
                            ScheduleDAGInstrs *DAG) {
  if (!TargetSU || SU == TargetSU)
    return false;
  return DAG->IsReachable(TargetSU, SU);
}

InstructionFlavor llvm::classifyFlavor(const MachineInstr *MI,
                                       const SIInstrInfo *SII) {
  if (!MI || MI->isDebugInstr())
    return InstructionFlavor::Other;

  unsigned Opc = MI->getOpcode();

  // Check for specific opcodes first.

  if (Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2 ||
      Opc == AMDGPU::TENSOR_LOAD_TO_LDS ||
      Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32 ||
      Opc == AMDGPU::GLOBAL_LOAD_ASYNC_TO_LDS_B32_SADDR)
    return InstructionFlavor::DMA;

  if (Opc == AMDGPU::ATOMIC_FENCE ||
      Opc == AMDGPU::S_WAIT_ASYNCCNT ||
      Opc == AMDGPU::S_WAIT_TENSORCNT ||
      Opc == AMDGPU::S_BARRIER_WAIT ||
      Opc == AMDGPU::S_BARRIER_SIGNAL_IMM)
    return InstructionFlavor::Fence;

  unsigned RepeatRate = SII->getRepeatRate(*MI);

  if (RepeatRate > 1 && SII->isVALU(*MI) && !SII->isMFMAorWMMA(*MI) &&
      !SII->isTRANS(*MI))
    return InstructionFlavor::MultiCycleVALU;

  // Check instruction categories.

  if (SII->isMFMAorWMMA(*MI))
    return InstructionFlavor::WMMA;

  if (SII->isTRANS(*MI))
    return InstructionFlavor::TRANS;

  if (SII->isVALU(*MI))
    return InstructionFlavor::SingleCycleVALU;

  if (SII->isDS(*MI))
    return InstructionFlavor::DS;

  if (SII->isFLAT(*MI) || SII->isFLATGlobal(*MI) || SII->isFLATScratch(*MI))
    return InstructionFlavor::VMEM;

  if (SII->isSALU(*MI))
    return InstructionFlavor::SALU;

  return InstructionFlavor::Other;
}

static unsigned getFlavorCycles(const MachineInstr *MI, InstructionFlavor F,
                                const SIInstrInfo *SII) {
  // WMMA: hardcoded to 8 cycles for now (gfx1250)
  // Note: Adding this to getRepeatRate() causes regressions elsewhere.
  if (F == InstructionFlavor::WMMA)
    return 8;

  return SII->getRepeatRate(*MI);
}

void RegionMixInfo::dumpMix(raw_ostream &OS, bool Detailed) const {
  OS << "Instruction Mix:\n";
  for (unsigned I = 0; I < NumFlavors; ++I) {
    InstructionFlavor F = static_cast<InstructionFlavor>(I);
    unsigned Total = getTotalCount(F);
    if (Total == 0)
      continue;
    OS << "  " << getFlavorName(F) << ": " << Total;
    if (Detailed)
      OS << " (cycles: " << getTotalCycles(F) << ")";
    OS << "\n";
  }
}

void RegionMixInfo::dumpReadyPending(raw_ostream &OS) const {
  OS << "Ready: ";
  bool First = true;
  for (unsigned I = 0; I < NumFlavors; ++I) {
    InstructionFlavor F = static_cast<InstructionFlavor>(I);
    unsigned Ready = getReadyCount(F);
    if (Ready == 0)
      continue;
    if (!First)
      OS << ", ";
    First = false;
    OS << Ready << " " << getFlavorName(F);
  }
  OS << "\nBlocked: ";
  First = true;
  for (unsigned I = 0; I < NumFlavors; ++I) {
    InstructionFlavor F = static_cast<InstructionFlavor>(I);
    unsigned Blocked = getPendingCount(F);
    if (Blocked == 0)
      continue;
    if (!First)
      OS << ", ";
    First = false;
    unsigned RemCycles = getRemainingCycles(F);
    OS << Blocked << " " << getFlavorName(F) << "(" << RemCycles << "c)";
  }
  OS << "\n";
}

AMDGPUMLSchedStrategy::AMDGPUMLSchedStrategy(const MachineSchedContext *C)
    : GCNSchedStrategy(C) {
  SchedStages.push_back(GCNSchedStageID::ILPInitialSchedule);
  SchedStages.push_back(GCNSchedStageID::PreRARematerialize);
  // Use more accurate GCN pressure trackers.
  UseGCNTrackers = true;
}

void AMDGPUMLSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  // ML scheduling strategy is only done top-down to support new resource
  // balancing heuristics.
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
  GCNSchedStrategy::initialize(DAG);

  CI.clear();
  CI.compute(DAG->MF);

  HWUInfo.resize((int)InstructionFlavor::NUM_FLAVORS);
  HWUInfo[(int)InstructionFlavor::DMA].IsAsync = true;

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HWUInfo[I].setType(I);
  }

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
    Latency = 8;

  MachineInstr *MI = SU->getInstr();
  unsigned RepeatRate = SII->getRepeatRate(*MI);

  if (RepeatRate > 1 && SII->isVALU(*MI) && !SII->isMFMAorWMMA(*MI) &&
      !SII->isTRANS(*MI)) {
    Latency = RepeatRate;
  }

  return Latency;
}

void AMDGPUMLSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  auto MI = SU->getInstr();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  DEBUG_WITH_TYPE("machine-scheduler-verbose", {
    dbgs() << "Scheduling: "; MI->dump();
    dbgs() << "\n\n";
  });
  if (SchedModel && SchedModel->hasInstrSchedModel()) {
    unsigned ReleaseAtCycle = 0;
    const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      ReleaseAtCycle = std::max(ReleaseAtCycle, (unsigned)PI->ReleaseAtCycle);
    }
    unsigned Latency = getHWUICyclesForInst(SU, SII, ReleaseAtCycle);

    auto *MI = SU->getInstr();
    InstructionFlavor Flavor = classifyFlavor(MI, SII);

    // FIXME
    assert(RegionPolicy.OnlyTopDown);
    GCNHazardRecognizer *HazardRec =
        static_cast<GCNHazardRecognizer *>(Top.HazardRec);
    bool IsHidden = HazardRec->inVALUShadow();

    bool FoundIt = false;
    for (auto &HWUI : HWUInfo) {
      if (HWUI.getType() == Flavor) {
        HWUI.schedule(SU, Latency);
        if (!IsHidden)
          HWUI.reduceRemainingExposed();
        FoundIt = true;
        break;
      }
    }

    assert(FoundIt);

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

  InstructionFlavor Flavor = classifyFlavor(MI, SII);
  MixInfo.markScheduled(SU, Flavor);

  GCNSchedStrategy::schedNode(SU, IsTopNode);
}

static void calculateHiddenLatency(SmallVectorImpl<HardwareUnitInfo> &HWUI,
                                   GCNHazardRecognizer *HazardRec) {
  unsigned WMMACycles = HWUI[(int)InstructionFlavor::WMMA].getTotalCycles();

  unsigned DSCycles = HWUI[(int)InstructionFlavor::DS].getTotalCycles();
  unsigned MultiVALUCycles =
      HWUI[(int)InstructionFlavor::MultiCycleVALU].getTotalCycles();
  unsigned SingleCycleVALUCycles =
      HWUI[(int)InstructionFlavor::SingleCycleVALU].getTotalCycles();
  unsigned SALUCount = HWUI[(int)InstructionFlavor::SALU].size();
  unsigned TRANSCycles = HWUI[(int)InstructionFlavor::TRANS].getTotalCycles();

  unsigned DSCount = HWUI[(int)InstructionFlavor::DS].size();
  unsigned WMMACount = HWUI[(int)InstructionFlavor::WMMA].size();
  unsigned MultiVALUCount = HWUI[(int)InstructionFlavor::MultiCycleVALU].size();
  unsigned EXPCount = HWUI[(int)InstructionFlavor::DS].size();
  unsigned SingleCycleVALUCount =
      HWUI[(int)InstructionFlavor::SingleCycleVALU].size();

  // FIXME -- what if we have multiple types of WMMA?
  unsigned ESlotCount = 0;
  unsigned ISlotCount = 0;
  unsigned ESlotForDS = 0;
  unsigned ISlotForTrans = 0;
  if (WMMACount) {
    SUnit *NextWMMA = HWUI[(int)InstructionFlavor::WMMA].getTargetSU();
    assert(NextWMMA);
    SmallVector<GCNHazardRecognizer::WMMASlotType, 8> WMMACoexecSlots;
    HazardRec->getWMMASlots(*NextWMMA->getInstr(), WMMACoexecSlots);
    for (auto &Slot : WMMACoexecSlots) {
      switch (Slot) {
      default:
        break;
      case GCNHazardRecognizer::WMMASlotType::MemCoExec0:
      case GCNHazardRecognizer::WMMASlotType::MemCoExec2: {
        ++ESlotForDS;
        ++ESlotCount;
        break;
      }
      case GCNHazardRecognizer::WMMASlotType::MemCoExec1:
      case GCNHazardRecognizer::WMMASlotType::MemCoExec3: {
        ++ESlotCount;
        break;
      }
      case GCNHazardRecognizer::WMMASlotType::ValuCoExec0: {
        ++ISlotForTrans;
        ++ISlotCount;
        break;
      }
      case GCNHazardRecognizer::WMMASlotType::ValuCoExec1:
      case GCNHazardRecognizer::WMMASlotType::ValuCoExec2:
      case GCNHazardRecognizer::WMMASlotType::ValuCoexecLastLdScale: {
        ++ISlotCount;
        break;
      }
      }
    }
  }

  unsigned WMMAESlot = WMMACount * ESlotCount;
  unsigned WMMAISlot = WMMACount * ISlotCount;
  unsigned WMMAESlotForDS = WMMACount * ESlotForDS;
  unsigned WMMAISlotForTRANS = WMMACount * ISlotForTrans;

  unsigned CoexecWithMultiVALU =
      MultiVALUCycles - HWUI[(int)InstructionFlavor::MultiCycleVALU].size();

  bool IsDSBound = DSCycles > WMMACycles + DSCycles + SingleCycleVALUCycles +
                                  TRANSCycles - 2 * WMMACount;

  if (!IsDSBound) {
    // If we are not DS Bound, then we cannot hide any of the WMMA or multi
    // cycle VALU
    HWUI[(int)InstructionFlavor::WMMA].setExposedCount(WMMACount);
    HWUI[(int)InstructionFlavor::MultiCycleVALU].setExposedCount(
        MultiVALUCount);

    unsigned DSWMMACoexecution = std::min(WMMAESlotForDS, DSCount);

    // Three main types of latency hiding:
    // 1. SALU / DS / VALU1c / EXP behinmg WMMA
    // 2. SALU / DS behind multi cycle VALU
    // 3. VALU1c behind EXP

    WMMAESlot -= DSWMMACoexecution;
    DSCount -= DSWMMACoexecution;

    if (DSCount) {
      unsigned DSMultiCoexecution = std::min(CoexecWithMultiVALU, DSCount);
      DSCount -= DSMultiCoexecution;
      CoexecWithMultiVALU -= DSMultiCoexecution;
    }

    HWUI[(int)InstructionFlavor::DS].setExposedCount(DSCount);

    unsigned SALUWMMACoexecution = std::min(WMMAESlot, SALUCount);
    SALUCount -= SALUWMMACoexecution;

    if (SALUCount) {
      unsigned SALUMultiCoexecution = std::min(CoexecWithMultiVALU, SALUCount);
      SALUCount -= SALUMultiCoexecution;
      CoexecWithMultiVALU -= SALUCount;
      WMMAESlot -= SALUMultiCoexecution;
    }

    HWUI[(int)InstructionFlavor::SALU].setExposedCount(SALUCount);

    unsigned EXPWMMACoexecution =
        true ? std::min(WMMAISlotForTRANS, EXPCount) : 0;

    WMMAISlot -= EXPWMMACoexecution;
    EXPCount -= EXPWMMACoexecution;

    HWUI[(int)InstructionFlavor::TRANS].setExposedCount(EXPCount);

    unsigned VALUWMMACoexecution = std::min(WMMAISlot, SingleCycleVALUCount);

    WMMAISlot -= VALUWMMACoexecution;
    SingleCycleVALUCount -= VALUWMMACoexecution;

    unsigned VALUEXPCoexecution = std::min(EXPCount, SingleCycleVALUCount);

    SingleCycleVALUCount -= VALUEXPCoexecution;

    HWUI[(int)InstructionFlavor::SingleCycleVALU].setExposedCount(
        SingleCycleVALUCount);
  }
}

void AMDGPUMLSchedStrategy::collectUse() {
  CollectedUse = true;
  SchedDSR.clear();
  SchedMFMA.clear();
  SchedTDM.clear();
  SchedEXP.clear();
  MixInfo.reset();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  for (auto &HWUI : HWUInfo) {
    HWUI.reset();
  }

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HWUInfo[I].reset();
    HWUInfo[I].setType(I);
  }

  HWUInfo[(int)InstructionFlavor::DMA].IsAsync = true;

  HWUInfo[(int)InstructionFlavor::WMMA].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::TRANS].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::DS].ProducesCoexecWindow = true;

  HWUInfo[(int)InstructionFlavor::WMMA].CoexecWindowSize = 6;
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].CoexecWindowSize = 3;
  HWUInfo[(int)InstructionFlavor::TRANS].CoexecWindowSize = 1;

  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  unsigned I = 0;
  unsigned PrevDSR = 0;
  unsigned PrevFence = 0;
  unsigned FencedDSRCount = 0;
  for (auto &SU : DAG->SUnits) {
    unsigned ReleaseAtCycle = 0;
    const MCSchedClassDesc *SC = DAG->getSchedClass(&SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      ReleaseAtCycle = std::max(ReleaseAtCycle, (unsigned)PI->ReleaseAtCycle);
    }
    unsigned Latency = getHWUICyclesForInst(&SU, SII, ReleaseAtCycle);

    auto *MI = SU.getInstr();
    InstructionFlavor Flavor = classifyFlavor(MI, SII);
    HWUInfo[(int)(Flavor)].insert(&SU, Latency);
    unsigned FlavorCycles = getFlavorCycles(MI, Flavor, SII);
    MixInfo.addSU(&SU, Flavor, FlavorCycles);

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

  HWUInfo[(unsigned)InstructionFlavor::Other].reset();

  if (IgnoreVALU) {
    HWUInfo[(unsigned)InstructionFlavor::SingleCycleVALU].reset();
    HWUInfo[(unsigned)InstructionFlavor::SALU].reset();
  }

  assert(RegionPolicy.OnlyTopDown);
  GCNHazardRecognizer *HazardRec =
      static_cast<GCNHazardRecognizer *>(Top.HazardRec);
  calculateHiddenLatency(HWUInfo, HazardRec);

  LLVM_DEBUG(dumpRegionSummary());
}

static void sortResources(SmallVectorImpl<HardwareUnitInfo> &HWUInfo,
                          SchedBoundary *Zone) {
  // Highest priority should be first.
  sort(HWUInfo, [](HardwareUnitInfo &A, HardwareUnitInfo &B) {
    // Both are either exposed or unexposed, prefer exec window producer
    if (ResourcePriorityToProducer) {
      if (A.ProducesCoexecWindow != B.ProducesCoexecWindow)
        return A.ProducesCoexecWindow;

      bool AIsExposed = A.getRemainingExposed() > 0;
      bool BIsExposed = B.getRemainingExposed() > 0;

      if (AIsExposed != BIsExposed)
        return AIsExposed;
    }

    else {
      bool AIsExposed = A.getRemainingExposed() > 0;
      bool BIsExposed = B.getRemainingExposed() > 0;

      if (AIsExposed != BIsExposed)
        return AIsExposed;

      if (A.ProducesCoexecWindow != B.ProducesCoexecWindow)
        return A.ProducesCoexecWindow;
    }

    if (ResourcePriorityCoexecWindowSize) {
      if (A.CoexecWindowSize != B.CoexecWindowSize) {
        return A.CoexecWindowSize > B.CoexecWindowSize;
      }
    }

    if (ResourcePriorityExposedCycles)
      // Give priority to the hardware unit with the most exposed cycles
      if (A.getRemainingExposed() != B.getRemainingExposed())
        return A.getRemainingExposed() > B.getRemainingExposed();

    // Less relevant tiebreakers
    // Total cycles
    if (A.getTotalCycles() != B.getTotalCycles())
      return A.getTotalCycles() > B.getTotalCycles();

    // In ties -- prefer the resource with longer latency instructions
    if (A.size() != B.size())
      return A.size() < B.size();

    // Default to HardwareUnitInfo order
    return A.Idx < B.Idx;
  });
}

void AMDGPUMLSchedStrategy::dumpRegionSummary() {
  MachineBasicBlock *BB = DAG->begin()->getParent();
  dbgs() << "\n=== Region: " << DAG->MF.getName() << " BB"
         << BB->getNumber() << " (" << DAG->SUnits.size() << " SUs) ===\n";

  MixInfo.dumpMix(dbgs(), /*Detailed=*/true);

  dbgs() << "\nHWUI Resource Pressure (sorted):\n";
  SmallVector<HardwareUnitInfo, 8> SortedHWUI = HWUInfo;
  sortResources(SortedHWUI, &Top);
  for (auto &HWUI : SortedHWUI) {
    if (HWUI.getTotalCycles() == 0)
      continue;

    StringRef Name = getFlavorName(HWUI.getType());
    dbgs() << "  [" << HWUI.Idx << "] " << Name << ": "
           << HWUI.getTotalCycles() << " cycles, " << HWUI.size() << " instrs\n";
  }
  dbgs() << "\n";
}

void AMDGPUMLSchedStrategy::dumpPickSummary(SUnit *SU, bool IsTopNode,
                                            SchedCandidate &Cand) {
  const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
  unsigned Cycle = IsTopNode ? Top.getCurrCycle() : Bot.getCurrCycle();

  dbgs() << "=== Pick @ Cycle " << Cycle << " ===\n";

  MixInfo.updateReadyCounts();
  MixInfo.dumpReadyPending(dbgs());

  InstructionFlavor Flavor = classifyFlavor(SU->getInstr(), SII);
  dbgs() << "Picked: SU(" << SU->NodeNum << ") ";
  SU->getInstr()->print(dbgs(), /*IsStandalone=*/true, /*SkipOpers=*/false,
                        /*SkipDebugLoc=*/true);
  dbgs() << " [" << getFlavorName(Flavor) << "]\n";

  dbgs() << "  Reason: ";
  if (LastAMDGPUReason != AMDGPUSchedReason::None)
    dbgs() << getReasonName(LastAMDGPUReason);
  else if (Cand.Reason != NoCand)
    dbgs() << GenericSchedulerBase::getReasonStr(Cand.Reason);
  else
    dbgs() << "Unknown";
  dbgs() << "\n\n";

  LastAMDGPUReason = AMDGPUSchedReason::None;
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

  unsigned LongLatVALU = SII->isTRANS(*MI) ? 0 : SII->getRepeatRate(*MI);
  if (LongLatVALU > 1 && (SchedMFMA.size() || SchedEXP.size())) {
    if (SchedMFMA.size()) {
      auto PrevMFMA = SchedMFMA[SchedMFMA.size() - 1];
      unsigned PrevMFMAIssue = PrevMFMA->TopReadyCycle;
      ReadyCycle = std::max(PrevMFMAIssue + PrevMFMA->Latency, ReadyCycle);
    }

    if (SchedEXP.size()) {
      auto PrevEXP = SchedEXP[SchedEXP.size() - 1];
      unsigned PrevEXPIssue = PrevEXP->TopReadyCycle;
      ReadyCycle = std::max(PrevEXPIssue + 2, ReadyCycle);
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
            //if (!OtherMO.isDef())
            //  continue;

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
      return HazardStates;
    }
  }

  if (ReadyCycle > CurrCycle) {
    SU->TopReadyCycle = ReadyCycle;
    auto Wait = ReadyCycle - CurrCycle;
    return Wait;
  }

  return 0;
}

static bool tryAsyncPipe(GenericSchedulerBase::SchedCandidate &TryCand,
                         GenericSchedulerBase::SchedCandidate &Cand,
                         SchedBoundary *Zone, ScheduleDAGInstrs *DAG,
                         const TargetRegisterInfo *TRI,
                         const SmallVectorImpl<SUnit *> &SchedMFMA,
                         const SmallVectorImpl<SUnit *> &SchedDSR,
                         const SmallVectorImpl<SUnit *> &SchedTDM,
                         const SmallVectorImpl<SUnit *> &SchedEXP,
                         bool IsPostRA) {

  auto getStallCycles =
      [&SchedTDM, &SchedDSR,
       &Zone](GenericSchedulerBase::SchedCandidate &SchedCand) -> unsigned {
    SUnit *SU = SchedCand.SU;
    unsigned ReadyCycle = SU->TopReadyCycle;
    unsigned CurrCycle = Zone->getCurrCycle();
    MachineInstr *MI = SU->getInstr();
    if (MI->getOpcode() == AMDGPU::TENSOR_LOAD_TO_LDS_D2) {
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
    if (ReadyCycle > CurrCycle)
      return ReadyCycle - CurrCycle;

    return 0;
  };

  auto isAsyncPipe =
      [](GenericSchedulerBase::SchedCandidate &SchedCand) -> bool {
    SUnit *SU = SchedCand.SU;
    MachineInstr *MI = SU->getInstr();
    unsigned Opc = MI->getOpcode();

    return Opc == AMDGPU::TENSOR_LOAD_TO_LDS_D2 ||
           Opc == AMDGPU::S_BARRIER_WAIT ||
           Opc == AMDGPU::S_BARRIER_SIGNAL_IMM || Opc == AMDGPU::ATOMIC_FENCE ||
           Opc == AMDGPU::S_WAIT_TENSORCNT;
  };

  bool CandIsAsync = isAsyncPipe(Cand);
  bool TryIsAsync = isAsyncPipe(TryCand);

  if (CandIsAsync == TryIsAsync)
    return false;

  if (CandIsAsync) {
    unsigned Stalls = getStallCycles(Cand);
    if (!Stalls) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical) {
        Cand.Reason = GenericSchedulerBase::RegCritical;
      }
      return true;
    }
    return false;
  }

  if (TryIsAsync) {
    unsigned Stalls = getStallCycles(TryCand);
    if (!Stalls) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
    return false;
  }

  return false;
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
    bool TryIsSingleCycleVALU =
        SII->isVALU(*TryMI) && !SII->isMFMAorWMMA(*TryMI) &&
        !SII->isTRANS(*TryMI) && (SII->getRepeatRate(*TryMI) <= 1);
    bool CandIsSingleCycleVALU =
        SII->isVALU(*CandMI) && !SII->isMFMAorWMMA(*CandMI) &&
        !SII->isTRANS(*CandMI) && !SII->isTRANS(*CandMI) &&
        (SII->getRepeatRate(*CandMI) <= 1);

    if (TryIsSingleCycleVALU == CandIsSingleCycleVALU) {
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

  if (CoexecSlot == -1) {
    unsigned TransWaits = HazardRec->getTRANS32HazardState();
    if (TransWaits) {
      return PreferNonTransVALU(TryCand, Cand);
    }
  }

  auto PreferTransVALU = [SII, &PreferNonTransVALU](GenericSchedulerBase::SchedCandidate &TryCand,
                                  GenericSchedulerBase::SchedCandidate &Cand) {
    MachineInstr *TryMI = TryCand.SU->getInstr();
    MachineInstr *CandMI = Cand.SU->getInstr();
    // We don't want to issue TRANS or CVT here as they (along with WMMA) will
    // clog the whole VALU unit for multiple cycles
    bool TryIsTRANS = SII->isTRANS(*TryMI);
    bool CandIsTTRANS = SII->isTRANS(*CandMI);


    if (!TryIsTRANS && !CandIsTTRANS) {
      return PreferNonTransVALU(TryCand, Cand);
    }

    if (TryIsTRANS == CandIsTTRANS) {
      return false;
    }

    if (CandIsTTRANS)
      if (Cand.Reason > GenericSchedulerBase::RegCritical) {
        Cand.Reason = GenericSchedulerBase::RegCritical;
      }

    if (TryIsTRANS) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
    }

    return true;
  };


  switch (CurrentSlot) {
    default:
      return false;
    
    case GCNHazardRecognizer::WMMASlotType::MemCoExec0:
    case GCNHazardRecognizer::WMMASlotType::MemCoExec2: {
      bool TryIsMem = SII->isFLATGlobal(*TryMI) || SII->isDS(*TryMI);
      bool CandIsMem = SII->isFLATGlobal(*CandMI) || SII->isDS(*CandMI);

      bool TryIsLargeCopy = TryMI->isCopy();
      bool CandIsLargeCopy = CandMI->isCopy();

      if (TryIsLargeCopy) {
        TryIsLargeCopy &= TRI->getRegSizeInBits(*DAG->MRI.getRegClass(
                              TryMI->getOperand(0).getReg())) > 64;
      }

      if (CandIsLargeCopy) {
        CandIsLargeCopy &= TRI->getRegSizeInBits(*DAG->MRI.getRegClass(
                               CandMI->getOperand(0).getReg())) > 64;
      }

      if (CandIsLargeCopy && TryIsLargeCopy)
        return false;

      if (!CandIsLargeCopy && !TryIsLargeCopy) {

        if (TryIsMem == CandIsMem)
          return false;

        if (CandIsMem)
          if (Cand.Reason > GenericSchedulerBase::RegCritical)
            Cand.Reason = GenericSchedulerBase::RegCritical;

        if (TryIsMem)
          TryCand.Reason = GenericSchedulerBase::RegCritical;

        return true;
      }

      if (CandIsLargeCopy)
        TryCand.Reason = GenericSchedulerBase::RegCritical;

      else if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::MemCoExec1:
    case GCNHazardRecognizer::WMMASlotType::MemCoExec3: {
      bool TryIsMem = SII->isFLATGlobal(*TryMI) || SII->isDS(*TryMI);
      bool CandIsMem = SII->isFLATGlobal(*CandMI) || SII->isDS(*CandMI);

      bool TryIsLargeCopy = TryMI->isCopy();
      bool CandIsLargeCopy = CandMI->isCopy();

      if (TryIsLargeCopy) {
        TryIsLargeCopy &= TRI->getRegSizeInBits(*DAG->MRI.getRegClass(
                              TryMI->getOperand(0).getReg())) > 64;
      }

      if (CandIsLargeCopy) {
        CandIsLargeCopy &= TRI->getRegSizeInBits(*DAG->MRI.getRegClass(
                               CandMI->getOperand(0).getReg())) > 64;
      }

      if (CandIsLargeCopy && TryIsLargeCopy)
        return false;

      if (!CandIsLargeCopy && !TryIsLargeCopy) {

        if (CandIsMem == TryIsMem) {
          return false;
        }

        if (!CandIsMem)
          if (Cand.Reason > GenericSchedulerBase::RegCritical)
            Cand.Reason = GenericSchedulerBase::RegCritical;

        if (!TryIsMem)
          TryCand.Reason = GenericSchedulerBase::RegCritical;

        return true;
      }

      if (CandIsLargeCopy)
        TryCand.Reason = GenericSchedulerBase::RegCritical;

      else if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    case GCNHazardRecognizer::WMMASlotType::ValuBlocked0: {
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
      return PreferNonTransVALU(TryCand, Cand);
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoExec0: {
      // We prefer 2 cycle TRANS here
      // FIXME -- should check that it is 2 cycle
      return PreferTransVALU(TryCand, Cand);
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoexecLastLdScale: {
      return PreferNonTransVALU(TryCand, Cand);
    }

    case GCNHazardRecognizer::WMMASlotType::ValuCoExec2: {
      return PreferTransVALU(TryCand, Cand);
    }
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Shadow Mix Heuristic
//===----------------------------------------------------------------------===//

/// Shadow Mix scheduling ensures WMMA instructions are only scheduled when
/// sufficient co-execution candidates (VALU and/or DS) are ready.
/// This enables the interleaved WMMA+VALU+DS pattern shown in optimal schedules.
///
/// Shadow Priority Rules (toggleable, prefer long-latency so short fills shadow):
/// 1a. WMMA over DS       (-amdgpu-shadow-priority-wmma-over-ds)
/// 1b. WMMA over SALU     (-amdgpu-shadow-priority-wmma-over-salu)
/// 2a. CVT over DS        (-amdgpu-shadow-priority-cvt-over-ds)
/// 2b. CVT over SALU      (-amdgpu-shadow-priority-cvt-over-salu)
/// 3.  TRANS32 over VALU1c (-amdgpu-shadow-priority-trans32-over-valu1c)
///
/// Co-exec enablement rules:
/// 4. If enough co-exec candidates ready -> no intervention
/// 5. Defer WMMA if not enough VALU/DS ready
/// 6. Prefer instructions that enable co-exec candidates
/// 7. Lookahead to find path to pending co-exec
///
/// Returns true if a decision was made, sets Reason on the preferred candidate.
static bool
tryShadowMix(GenericSchedulerBase::SchedCandidate &TryCand,
             GenericSchedulerBase::SchedCandidate &Cand,
             SchedBoundary *Zone, RegionMixInfo &MixInfo,
             const SIInstrInfo *SII, ScheduleDAGInstrs *DAG,
             AMDGPUSchedReason &OutReason) {
  if (!EnableShadowMix)
    return false;

  MixInfo.updateReadyCounts();

  unsigned ReadyVALU1c = MixInfo.getReadyCount(InstructionFlavor::SingleCycleVALU);
  unsigned ReadyDS = MixInfo.getReadyCount(InstructionFlavor::DS);
  unsigned ReadySALU = MixInfo.getReadyCount(InstructionFlavor::SALU);
  unsigned ReadyWMMA = MixInfo.getReadyCount(InstructionFlavor::WMMA);

  // FIXME: should these values be determined by calculateHiddenLatency
  unsigned RequiredVALU1c = ShadowMixWMMAMinVALU1c;
  unsigned RequiredDS = ShadowMixWMMAMinDS;
  unsigned RequiredSALU = ShadowMixWMMAMinSALU;
  unsigned RequiredWMMA = ShadowMixWMMAMinWMMA;

  InstructionFlavor TryFlavor = classifyFlavor(TryCand.SU->getInstr(), SII);
  InstructionFlavor CandFlavor = classifyFlavor(Cand.SU->getInstr(), SII);

  bool TryIsWMMA = (TryFlavor == InstructionFlavor::WMMA);
  bool CandIsWMMA = (CandFlavor == InstructionFlavor::WMMA);
  bool TryIsCVT = (TryFlavor == InstructionFlavor::MultiCycleVALU);
  bool CandIsCVT = (CandFlavor == InstructionFlavor::MultiCycleVALU);
  bool TryIsDS = (TryFlavor == InstructionFlavor::DS);
  bool CandIsDS = (CandFlavor == InstructionFlavor::DS);
  bool TryIsSALU = (TryFlavor == InstructionFlavor::SALU);
  bool CandIsSALU = (CandFlavor == InstructionFlavor::SALU);
  bool TryIsVALU1c = (TryFlavor == InstructionFlavor::SingleCycleVALU);
  bool CandIsVALU1c = (CandFlavor == InstructionFlavor::SingleCycleVALU);
  bool TryIsTRANS32 = (TryFlavor == InstructionFlavor::TRANS);
  bool CandIsTRANS32 = (CandFlavor == InstructionFlavor::TRANS);

  // Check if we have enough co-exec candidates for WMMA
  bool HaveEnoughVALU1c = (RequiredVALU1c == 0) || (ReadyVALU1c >= RequiredVALU1c);
  bool HaveEnoughDS = (RequiredDS == 0) || (ReadyDS >= RequiredDS);
  bool HaveEnoughSALU = (RequiredSALU == 0) || (ReadySALU >= RequiredSALU);
  bool HaveEnoughCoexec = HaveEnoughVALU1c && HaveEnoughDS && HaveEnoughSALU;
  bool HaveEnoughWMMA = (RequiredWMMA == 0) || (ReadyWMMA >= RequiredWMMA);
  //errs() << "HaveEnoughWMMA: " << HaveEnoughWMMA << "\n";

  // Helper lambda for shadow priority decisions
  auto preferFirst = [&](bool TryIsFirst, AMDGPUSchedReason Reason,
                         const char *FirstName, const char *SecondName) -> bool {
    if (TryIsFirst) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = Reason;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer " << FirstName << " over "
                        << SecondName << " (will fill shadow)\n");
      return true;
    } else {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = Reason;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer " << FirstName << " over "
                        << SecondName << " (will fill shadow)\n");
      return true;
    }
  };

  if (ShadowMixRules) {
    // Shadow Priority Rules: prefer long-latency so short ones fill shadow
    // slots. Each rule is independently toggleable for debugging.

    // Rule 1a: WMMA over DS
    if (ShadowPriorityWMMAOverDS && TryIsWMMA != CandIsWMMA) {
      if ((TryIsWMMA && CandIsDS) || (TryIsDS && CandIsWMMA))
        return preferFirst(TryIsWMMA,
                           AMDGPUSchedReason::ShadowPriorityWMMAOverDS, "WMMA",
                           "DS");
    }

    // Rule 1b: WMMA over SALU
    if (ShadowPriorityWMMAOverSALU && TryIsWMMA != CandIsWMMA) {
      if ((TryIsWMMA && CandIsSALU) || (TryIsSALU && CandIsWMMA))
        return preferFirst(TryIsWMMA,
                           AMDGPUSchedReason::ShadowPriorityWMMAOverSALU,
                           "WMMA", "SALU");
    }

    // Rule 2a: CVT over DS
    if (ShadowPriorityCVTOverDS && TryIsCVT != CandIsCVT) {
      if ((TryIsCVT && CandIsDS) || (TryIsDS && CandIsCVT))
        return preferFirst(TryIsCVT, AMDGPUSchedReason::ShadowPriorityCVTOverDS,
                           "CVT", "DS");
    }

    // Rule 2b: CVT over SALU
    if (ShadowPriorityCVTOverSALU && TryIsCVT != CandIsCVT) {
      if ((TryIsCVT && CandIsSALU) || (TryIsSALU && CandIsCVT))
        return preferFirst(TryIsCVT,
                           AMDGPUSchedReason::ShadowPriorityCVTOverSALU, "CVT",
                           "SALU");
    }

    // Rule 3: TRANS32 (v_exp etc) over 1-cycle VALU
    if (ShadowPriorityTRANS32OverVALU1c && TryIsTRANS32 != CandIsTRANS32) {
      if ((TryIsTRANS32 && CandIsVALU1c) || (TryIsVALU1c && CandIsTRANS32))
        return preferFirst(TryIsTRANS32,
                           AMDGPUSchedReason::ShadowPriorityTRANS32OverVALU,
                           "TRANS32", "VALU1c");
    }

    // Rule 3b: When filling TRANS32 shadow, prefer VALU1c over SALU
    // This reserves SALU for WMMA/CVT shadows where it's more valuable.
    if (ShadowPreferVALU1cOverSALUForTRANS && TryIsVALU1c != CandIsVALU1c) {
      if ((TryIsVALU1c && CandIsSALU) || (TryIsSALU && CandIsVALU1c))
        return preferFirst(
            TryIsVALU1c, AMDGPUSchedReason::ShadowPreferVALU1cOverSALUForTRANS,
            "VALU1c", "SALU");
    }
  }

  // Rule 4a: If we have enough co-exec candidates, no further intervention needed
  if (HaveEnoughCoexec && HaveEnoughWMMA)
    return false;

  // Rule 5: Defer WMMA if not enough co-exec candidates ready
  if (!HaveEnoughCoexec && TryIsWMMA != CandIsWMMA) {
    // Prefer the non-WMMA candidate
    if (TryIsWMMA) {
      // Cand is non-WMMA, prefer it
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = AMDGPUSchedReason::ShadowDeferWMMA;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Deferring WMMA (VALU1c=" << ReadyVALU1c
                        << "/" << RequiredVALU1c << ", DS=" << ReadyDS
                        << "/" << RequiredDS << ")\n");
      return true;
    } else {
      // TryCand is non-WMMA, prefer it
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = AMDGPUSchedReason::ShadowDeferWMMA;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Deferring WMMA (VALU1c=" << ReadyVALU1c
                        << "/" << RequiredVALU1c << ", DS=" << ReadyDS
                        << "/" << RequiredDS << ")\n");
      return true;
    }
  }

  // Rule 5b: Defer TRANS32 if not enough VALU ready (optional)
  if (ShadowDeferTRANS32 && HaveEnoughWMMA &&  TryIsTRANS32 != CandIsTRANS32) {
    unsigned RequiredForTRANS = ShadowMixTRANS32MinVALU1c;
    if (ReadyVALU1c < RequiredForTRANS) {
      // Prefer the non-TRANS32 candidate
      if (TryIsTRANS32) {
        // Cand is non-TRANS32, prefer it
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
        OutReason = AMDGPUSchedReason::ShadowDeferTRANS32;
        LLVM_DEBUG(dbgs() << "  ShadowMix: Deferring TRANS32 (VALU1c=" << ReadyVALU1c
                          << "/" << RequiredForTRANS << ")\n");
        return true;
      } else {
        // TryCand is non-TRANS32, prefer it
        TryCand.Reason = GenericSchedulerBase::RegCritical;
        OutReason = AMDGPUSchedReason::ShadowDeferTRANS32;
        LLVM_DEBUG(dbgs() << "  ShadowMix: Deferring TRANS32 (VALU1c=" << ReadyVALU1c
                          << "/" << RequiredForTRANS << ")\n");
        return true;
      }
    }
  }

  // Both are WMMA or both are non-WMMA
  /*if (!HaveEnoughCoexec && TryIsWMMA && CandIsWMMA) {
    // Both WMMA - no preference, but we shouldn't schedule either yet
    // This will be handled by other heuristics or we'll stall
    return false;
  }*/

  // Rule 6: Neither is WMMA, prefer the one that enables more co-exec candidates
  // Check which flavor we're short on and prioritize enabling that
  InstructionFlavor NeededFlavor = InstructionFlavor::WMMA;
  if (HaveEnoughWMMA) {
    NeededFlavor = InstructionFlavor::SingleCycleVALU;
    if (!HaveEnoughDS && HaveEnoughVALU1c) {
      NeededFlavor = InstructionFlavor::DS;
    } else if (!HaveEnoughVALU1c && !HaveEnoughDS) {
      // Short on both - prioritize VALU since it fills more co-exec slots
      NeededFlavor = InstructionFlavor::SingleCycleVALU;
    }
  }

  // First check direct enablement (O(succs) - cheap)
  unsigned TryEnables = countDirectlyEnabledByFlavor(TryCand.SU, NeededFlavor, SII);
  unsigned CandEnables = countDirectlyEnabledByFlavor(Cand.SU, NeededFlavor, SII);

  if (TryEnables != CandEnables) {
    StringRef FlavorName = getFlavorShortName(NeededFlavor);
    if (TryEnables > CandEnables) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = AMDGPUSchedReason::ShadowEnableDirect;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer TryCand (enables " << TryEnables
                        << " vs " << CandEnables << " " << FlavorName << ")\n");
      return true;
    } else {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      OutReason = AMDGPUSchedReason::ShadowEnableDirect;
      LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer Cand (enables " << CandEnables
                        << " vs " << TryEnables << " " << FlavorName << ")\n");
      return true;
    }
  }

  // Rule 7: Neither directly enables needed flavor - use lookahead
  auto [NearestTarget, Cost] = findNearestPendingByFlavor(
      MixInfo, NeededFlavor, ShadowMixLookaheadDepth, ShadowMixMaxBlockingCost,
      ShadowMixMaxVisited, ShadowMixMaxCandidates);

  if (NearestTarget) {
    bool TryHelps = wouldHelpEnable(TryCand.SU, NearestTarget, DAG);
    bool CandHelps = wouldHelpEnable(Cand.SU, NearestTarget, DAG);

    if (TryHelps != CandHelps) {
      StringRef FlavorName = getFlavorShortName(NeededFlavor);
      if (TryHelps) {
        TryCand.Reason = GenericSchedulerBase::RegCritical;
        OutReason = AMDGPUSchedReason::ShadowEnableLookahead;
        LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer TryCand (on path to "
                          << FlavorName << ", cost=" << Cost << ")\n");
        return true;
      } else {
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
        OutReason = AMDGPUSchedReason::ShadowEnableLookahead;
        LLVM_DEBUG(dbgs() << "  ShadowMix: Prefer Cand (on path to "
                          << FlavorName << ", cost=" << Cost << ")\n");
        return true;
      }
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

  auto IsCandidateResource = [&HWUInfo](unsigned ResourceIdx) {
    // unsigned MaxAvailableLat =
    // Zone->findMaxLatency(Zone->Available.elements());
    HardwareUnitInfo HWUI = HWUInfo[ResourceIdx];
    // unsigned CriticalUsage = HWUI.getTotalCycles();

    if (HWUI.getRemainingExposed() == 0 && !HWUI.ProducesCoexecWindow)
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
                                ScheduleDAGInstrs *DAG, bool IsPostRA,
                                RegionMixInfo &MixInfo) {
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

    // unsigned MaxAvailableLat =
    // Zone->findMaxLatency(Zone->Available.elements()); unsigned CriticalUsage
    // = HWUI.getTotalCycles();

    // if (MaxAvailableLat > CriticalUsage)
    //   return false;

    if (HWUI.getRemainingExposed() == 0 && !HWUI.ProducesCoexecWindow)
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
    DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "PhysReg\n");
    return TryCand.Reason != NoCand;
  }

  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    if (tryAsyncPipe(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR,
                     SchedTDM, SchedEXP, false)) {
      return TryCand.Reason != NoCand;
    }
  }

  // Avoid exceeding the target's limit.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.Excess, Cand.RPDelta.Excess, TryCand, Cand,
                  RegExcess, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  /*
  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;*/

  if (SameBoundary) {
    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                TryCand, Cand, Stall)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", {
        unsigned TryStall = getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM, SchedEXP, false);
        unsigned CandStall = getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM, SchedEXP, false);
        dbgs() << "Stall, Try: " << TryStall << ", Cand: " << CandStall << "\n";
      });
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ValuCoexec\n");
      LastAMDGPUReason = AMDGPUSchedReason::WMMACoexec;
      return TryCand.Reason != NoCand;
    }

    // Shadow Mix: Ensure sufficient VALU ready before scheduling WMMA.
    // This enables interleaved WMMA+VALU co-execution patterns.
    const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
    if (tryShadowMix(TryCand, Cand, Zone, MixInfo, SII, DAG, LastAMDGPUReason)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ShadowMix\n");
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo, Zone);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, false,
                            MixInfo)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResource\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep Async\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }
  }

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

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
      if (PrevOp == AMDGPU::ATOMIC_FENCE || PrevOp == AMDGPU::S_BARRIER_SIGNAL_IMM || PrevOp == AMDGPU::S_BARRIER_WAIT) {
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
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg)) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "PhysReg\n");
    return TryCand.Reason != NoCand;
  }
  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    if (tryAsyncPipe(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR,
                     SchedTDM, SchedEXP, false)) {
      return TryCand.Reason != NoCand;
    }
  }

  // Avoid exceeding the target's limit.
  if (DAG->isTrackingPressure() && tryPressure(TryCand.RPDelta.Excess,
                                               Cand.RPDelta.Excess,
                                               TryCand, Cand, RegExcess, TRI,
                                               DAG->MF))
    return TryCand.Reason != NoCand;

  /*
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

  if (SameBoundary) {

    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone,
                                      DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG,
                                      TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, false),
                TryCand, Cand, Stall)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", {
        unsigned TryStall = getLatencyStallCycles(TryCand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM, SchedEXP, false);
        unsigned CandStall = getLatencyStallCycles(Cand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM, SchedEXP, false);
        dbgs() << "Stall, Try: " << TryStall << ", Cand: " << CandStall << "\n";
      });
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR,
                          SchedTDM, SchedEXP, false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ValuCoexec\n");
      LastAMDGPUReason = AMDGPUSchedReason::WMMACoexec;
      return TryCand.Reason != NoCand;
    }

    // Shadow Mix: Ensure sufficient VALU ready before scheduling WMMA.
    // This enables interleaved WMMA+VALU co-execution patterns.
    const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
    if (tryShadowMix(TryCand, Cand, Zone, MixInfo, SII, DAG, LastAMDGPUReason)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ShadowMix\n");
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo, Zone);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, false,
                            MixInfo)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResource\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, false,
                            MixInfo)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResource\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                      false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                      false)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep Async\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    //if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
    //    tryLatency(TryCand, Cand, *Zone))
    //  return TryCand.Reason != NoCand;

  }
  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

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
                 Cluster)) {
                 DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Cluster\n");
    return TryCand.Reason != NoCand;
                 }
*/
  if (SameBoundary) {
    /*
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak)) {
                  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Weak\n");
      return TryCand.Reason != NoCand;
                }
                */
  }

  // Avoid increasing the max pressure of the entire region.
  //if (DAG->isTrackingPressure() &&
  //    tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
  //                Cand, RegMax, TRI, DAG->MF)) {
   //                DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "RP\n");
   // return TryCand.Reason != NoCand;
   //               }

  // Fall through to original instruction order.
  if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
      (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NID\n");
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
  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Checking Available:\n");
  ReadyQueue &AQ = Zone.Available;
  for (SUnit *SU : AQ) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", SU->getInstr()->dump());
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    tryCandidateBalanced(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NewBest!\n");
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
  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Checking Pending:\n");
  ReadyQueue &PQ = Zone.Pending;
  for (SUnit *SU : PQ) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", SU->getInstr()->dump());
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, IsBottomUp);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    AMDGPUMLSchedStrategy::tryPendingCandidate(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NewBest!\n");
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
  SchedCandidate *PickedCand = nullptr;
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
        PickedCand = &TopCand;
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
        PickedCand = &BotCand;
      }
      IsTopNode = false;
    } else {
      SU = pickNodeBidirectional(IsTopNode, PickedPending);
      PickedCand = IsTopNode ? &TopCand : &BotCand;
    }
  } while (SU->isScheduled);

  LLVM_DEBUG(if (PickedCand) dumpPickSummary(SU, IsTopNode, *PickedCand));

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
      DEBUG_WITH_TYPE("machine-scheduler-verbose", {
        unsigned TryStall = getLatencyStallCycles(
            TryCand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA,
            SchedDSR, SchedTDM, SchedEXP, false);
        unsigned CandStall = getLatencyStallCycles(
            Cand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR,
            SchedTDM, SchedEXP, false);
        dbgs() << "Stall, Try: " << TryStall << ", Cand: " << CandStall << "\n";
      });
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ValuCoexec\n");
      LastAMDGPUReason = AMDGPUSchedReason::WMMACoexec;
      return TryCand.Reason != NoCand;
    }

    // Shadow Mix: Ensure sufficient VALU ready before scheduling WMMA.
    // This enables interleaved WMMA+VALU co-execution patterns.
    const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
    if (tryShadowMix(TryCand, Cand, Zone, MixInfo, SII, DAG, LastAMDGPUReason)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ShadowMix\n");
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo, Zone);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, true, MixInfo)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResource\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDepAsync\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
        tryLatency(TryCand, Cand, *Zone)) {
          DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Latency\n");
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
                  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Cluster\n");
      return TryCand.Reason != NoCand;
                }
  }

  if (SameBoundary) {
    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
          DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NID\n");
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
      DEBUG_WITH_TYPE("machine-scheduler-verbose", {
        unsigned TryStall = getLatencyStallCycles(
            TryCand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA,
            SchedDSR, SchedTDM, SchedEXP, false);
        unsigned CandStall = getLatencyStallCycles(
            Cand.SU, Zone->getCurrCycle(), Zone, DAG, TRI, SchedMFMA, SchedDSR,
            SchedTDM, SchedEXP, false);
        dbgs() << "Stall, Try: " << TryStall << ", Cand: " << CandStall << "\n";
      });
      return TryCand.Reason != NoCand;
    }

    if (tryVALUCoexecSlot(TryCand, Cand, Zone, DAG, TRI, SchedMFMA, SchedDSR, SchedTDM,
                                      SchedEXP, true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ValuCoexec\n");
      LastAMDGPUReason = AMDGPUSchedReason::WMMACoexec;
      return TryCand.Reason != NoCand;
    }

    // Shadow Mix: Ensure sufficient VALU ready before scheduling WMMA.
    // This enables interleaved WMMA+VALU co-execution patterns.
    const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
    if (tryShadowMix(TryCand, Cand, Zone, MixInfo, SII, DAG, LastAMDGPUReason)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "ShadowMix\n");
      return TryCand.Reason != NoCand;
    }

    sortResources(HWUInfo, Zone);
    if (tryCriticalResource(TryCand, Cand, Zone, HWUInfo, DAG, true, MixInfo)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResource\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, false, HWUInfo, DAG,
                                       true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }

    if (tryCriticalResourceDependency(TryCand, Cand, Zone, true, HWUInfo, DAG,
                                       true)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "CritResourceDep Async\n");
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }
  }

  if (SameBoundary) {
    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
          DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NID\n");
      TryCand.Reason = NodeOrder;
      return true;
    }
  }

  return false;
}

void AMDGPUMLPostSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
   DEBUG_WITH_TYPE("machine-scheduler-verbose", {
     dbgs() << "Scheduling: "; DAG->dumpNode(*SU);
     dbgs() << "\n\n";
   });
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);
  if (SchedModel && SchedModel->hasInstrSchedModel()) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(SU);

    unsigned ReleaseAtCycle = 0;
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      ReleaseAtCycle = std::max(ReleaseAtCycle, (unsigned)PI->ReleaseAtCycle);
    }
    unsigned Latency = getHWUICyclesForInst(SU, SII, ReleaseAtCycle);

    auto *MI = SU->getInstr();
    InstructionFlavor Flavor = classifyFlavor(MI, SII);

    assert(RegionPolicy.OnlyTopDown);
    GCNHazardRecognizer *HazardRec =
        static_cast<GCNHazardRecognizer *>(Top.HazardRec);
    bool IsHidden = HazardRec->inVALUShadow();

    bool FoundIt = false;
    for (auto &HWUI : HWUInfo) {
      if (HWUI.getType() == Flavor) {
        HWUI.schedule(SU, Latency);
        if (!IsHidden)
          HWUI.reduceRemainingExposed();
        FoundIt = true;
        break;
      }
    }

    assert(FoundIt);
  }

  auto MI = SU->getInstr();

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

  InstructionFlavor Flavor = classifyFlavor(MI, SII);
  MixInfo.markScheduled(SU, Flavor);

  PostGenericScheduler::schedNode(SU, IsTopNode);
}


void AMDGPUMLPostSchedStrategy::collectUse() {
  CollectedUse = true;
  SchedDSR.clear();
  SchedMFMA.clear();
  SchedTDM.clear();
  SchedEXP.clear();
  MixInfo.reset();
  const SIInstrInfo *SII = reinterpret_cast<const SIInstrInfo *>(DAG->TII);

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HWUInfo[I].reset();
    HWUInfo[I].setType(I);
  }

  HWUInfo[(int)InstructionFlavor::DMA].IsAsync = true;

  HWUInfo[(int)InstructionFlavor::WMMA].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::TRANS].ProducesCoexecWindow = true;
  HWUInfo[(int)InstructionFlavor::DS].ProducesCoexecWindow = true;

  HWUInfo[(int)InstructionFlavor::WMMA].CoexecWindowSize = 6;
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].CoexecWindowSize = 3;
  HWUInfo[(int)InstructionFlavor::TRANS].CoexecWindowSize = 1;

  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  unsigned I = 0;
  unsigned PrevDSR = 0;
  unsigned PrevFence = 0;
  unsigned FencedDSRCount = 0;
  for (auto &SU : DAG->SUnits) {
    unsigned ReleaseAtCycle = 0;
    const MCSchedClassDesc *SC = DAG->getSchedClass(&SU);
    for (TargetSchedModel::ProcResIter
             PI = SchedModel->getWriteProcResBegin(SC),
             PE = SchedModel->getWriteProcResEnd(SC);
         PI != PE; ++PI) {
      ReleaseAtCycle = std::max(ReleaseAtCycle, (unsigned)PI->ReleaseAtCycle);
    }
    unsigned Latency = getHWUICyclesForInst(&SU, SII, ReleaseAtCycle);

    auto *MI = SU.getInstr();
    InstructionFlavor Flavor = classifyFlavor(MI, SII);
    HWUInfo[(int)Flavor].insert(&SU, Latency);

    unsigned FlavorCycles = getFlavorCycles(MI, Flavor, SII);
    MixInfo.addSU(&SU, Flavor, FlavorCycles);

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

  HWUInfo[(unsigned)InstructionFlavor::Other].reset();
  if (IgnoreVALU) {
    HWUInfo[(unsigned)InstructionFlavor::SingleCycleVALU].reset();
    HWUInfo[(unsigned)InstructionFlavor::SALU].reset();
  }

  assert(RegionPolicy.OnlyTopDown);
  GCNHazardRecognizer *HazardRec =
      static_cast<GCNHazardRecognizer *>(Top.HazardRec);
  calculateHiddenLatency(HWUInfo, HazardRec);

  LLVM_DEBUG(dumpRegionSummary());
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
  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "New region\n");
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
  PostGenericScheduler::initialize(DAG);

  HWUInfo.resize((int)InstructionFlavor::NUM_FLAVORS);
  HWUInfo[(int)InstructionFlavor::DMA].IsAsync = true;

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HWUInfo[I].setType(I);
  }

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
  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Checking Available\n");
  for (SUnit *SU : Q) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", DAG->dumpNode(*SU));
    SchedCandidate TryCand(Cand.Policy);
    TryCand.SU = SU;
    TryCand.AtTop = Zone.isTop();
    TryCand.initResourceDelta(DAG, SchedModel);
    if (AMDGPUMLPostSchedStrategy::tryCandidate(Cand, TryCand, &Zone)) {
      IsPending = false;
      Cand.setBest(TryCand);
       DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NewBest\n");
      LLVM_DEBUG(traceCandidate(Cand));
    }
  }

  ReadyQueue &PQ = Zone.Pending;
  DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "Checking Pending\n");
  for (SUnit *SU : PQ) {
    DEBUG_WITH_TYPE("machine-scheduler-verbose", DAG->dumpNode(*SU));
    SchedCandidate TryCand(Cand.Policy);
    TryCand.SU = SU;
    TryCand.AtTop = Zone.isTop();
    TryCand.initResourceDelta(DAG, SchedModel);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    if (tryPendingCandidate(Cand, TryCand, ZoneArg)) {
      DEBUG_WITH_TYPE("machine-scheduler-verbose", dbgs() << "NewBest\n");
      IsPending = true;
      Cand.setBest(TryCand);
      LLVM_DEBUG(traceCandidate(Cand));
    }
  }
}

void AMDGPUMLPostSchedStrategy::dumpRegionSummary() {
  MachineBasicBlock *BB = DAG->begin()->getParent();
  dbgs() << "\n=== PostRA Region: " << DAG->MF.getName() << " BB"
         << BB->getNumber() << " (" << DAG->SUnits.size() << " SUs) ===\n";

  MixInfo.dumpMix(dbgs(), /*Detailed=*/true);

  dbgs() << "\nHWUI Resource Pressure (sorted):\n";
  SmallVector<HardwareUnitInfo, 8> SortedHWUI = HWUInfo;
  sortResources(SortedHWUI, &Top);
  for (auto &HWUI : SortedHWUI) {
    if (HWUI.getTotalCycles() == 0)
      continue;
    StringRef Name = getFlavorName(HWUI.getType());
    dbgs() << "  [" << HWUI.Idx << "] " << Name << ": "
           << HWUI.getTotalCycles() << " cycles, " << HWUI.size() << " instrs\n";
  }
  dbgs() << "\n";
}

void AMDGPUMLPostSchedStrategy::dumpPickSummary(SUnit *SU, bool IsTopNode,
                                                SchedCandidate &Cand) {
  const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
  unsigned Cycle = IsTopNode ? Top.getCurrCycle() : Bot.getCurrCycle();

  dbgs() << "=== PostRA Pick @ Cycle " << Cycle << " ===\n";

  MixInfo.updateReadyCounts();
  MixInfo.dumpReadyPending(dbgs());

  InstructionFlavor Flavor = classifyFlavor(SU->getInstr(), SII);
  dbgs() << "Picked: SU(" << SU->NodeNum << ") ";
  SU->getInstr()->print(dbgs(), /*IsStandalone=*/true, /*SkipOpers=*/false,
                        /*SkipDebugLoc=*/true);
  dbgs() << " [" << getFlavorName(Flavor) << "]\n";

  dbgs() << "  Reason: ";
  if (LastAMDGPUReason != AMDGPUSchedReason::None)
    dbgs() << getReasonName(LastAMDGPUReason);
  else if (Cand.Reason != NoCand)
    dbgs() << GenericSchedulerBase::getReasonStr(Cand.Reason);
  else
    dbgs() << "Unknown";
  dbgs() << "\n\n";

  LastAMDGPUReason = AMDGPUSchedReason::None;
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
  SchedCandidate *PickedCand = nullptr;
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
      PickedCand = &BotCand;
    }
    IsTopNode = false;
  } else if (RegionPolicy.OnlyTopDown) {
    SU = pickOnlyChoice(Top, SchedModel);
    if (!SU) {
      CandPolicy NoPolicy;
      TopCand.reset(NoPolicy);
      // Set the top-down policy based on the state of the current top zone
      // and the instructions outside the zone, including the bottom zone.
      setPolicy(TopCand.Policy, /*IsPostRA=*/true, Top, nullptr);
      pickNodeFromQueue(Top, TopCand, IsPending);
      assert(TopCand.Reason != NoCand && "failed to find a candidate");

      SU = TopCand.SU;
      PickedCand = &TopCand;
    }
    IsTopNode = true;

  } else {
    SU = pickNodeBidirectional(IsTopNode, IsPending);
    PickedCand = IsTopNode ? &TopCand : &BotCand;
  }
  assert(!SU->isScheduled && "SUnit scheduled twice.");

  LLVM_DEBUG(if (PickedCand) dumpPickSummary(SU, IsTopNode, *PickedCand));

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