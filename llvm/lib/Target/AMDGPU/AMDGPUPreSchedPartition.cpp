//===-- AMDGPUPreSchedPartition.cpp - Pre-scheduler instruction partitioner ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Partitions instructions in loop bodies into groups based on WMMA boundaries.
// Assigns partition IDs (0-3) to WMMA/TRANS/DS/VALU instructions, optionally
// reordering them and inserting scheduling barriers to preserve grouping.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "GCNSubtarget.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "SIInstrInfo.h"
#include "SIRegisterInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <queue>

using namespace llvm;

#define DEBUG_TYPE "amdgpu-pre-sched-partition"

static cl::opt<bool> EnablePreSchedPartition(
    "amdgpu-pre-sched-partition",
    cl::desc("Enable pre-scheduling partitioning for MSB optimization"),
    cl::init(false), cl::Hidden);

static cl::opt<bool>
    DumpPartitionInfo("amdgpu-pre-sched-partition-dump",
                      cl::desc("Dump partition assignment information"),
                      cl::init(false), cl::Hidden);

static cl::opt<bool> InsertPartitionBarriers(
    "amdgpu-pre-sched-partition-barriers",
    cl::desc("Insert SCHED_BARRIER at partition boundaries"), cl::init(false),
    cl::Hidden);

static cl::opt<bool> SetPartitionHints(
    "amdgpu-pre-sched-partition-hints",
    cl::desc("Set MSB block allocation hints on partitioned registers"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> EnableReordering(
    "amdgpu-pre-sched-partition-reorder",
    cl::desc("Enable DAG-based instruction reordering by partition"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RedistributeTRANS(
    "amdgpu-pre-sched-partition-trans",
    cl::desc("Redistribute TRANS/V_EXP across first two WMMA partitions"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RedistributePKVALU(
    "amdgpu-pre-sched-partition-pkvalu",
    cl::desc("Redistribute packed VALU to balance with TRANS/V_EXP counts"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> VerboseDAGAnalysis(
    "amdgpu-pre-sched-partition-verbose-dag",
    cl::desc("Print verbose DAG analysis for partition planning"),
    cl::init(false), cl::Hidden);

namespace {

// Scheduling DAG that respects partition assignments when reordering.
class PartitionDAG : public ScheduleDAGInstrs {
public:
  PartitionDAG(MachineFunction &MF, const MachineLoopInfo *MLI,
               DenseMap<MachineInstr *, unsigned> &PartMap)
      : ScheduleDAGInstrs(MF, MLI), InstrToPartition(PartMap) {}

  void schedule() override {}

  bool reorderByPartition(MachineBasicBlock *MBB,
                          MachineBasicBlock::iterator Begin,
                          MachineBasicBlock::iterator End);

private:
  DenseMap<MachineInstr *, unsigned> &InstrToPartition;

  unsigned getPartition(const SUnit *SU) const {
    if (!SU || !SU->getInstr())
      return UINT_MAX;
    auto It = InstrToPartition.find(SU->getInstr());
    return It != InstrToPartition.end() ? It->second : UINT_MAX;
  }
};

// Topological sort with partition as primary key, ready time as secondary.
bool PartitionDAG::reorderByPartition(MachineBasicBlock *MBB,
                                      MachineBasicBlock::iterator Begin,
                                      MachineBasicBlock::iterator End) {
  startBlock(MBB);
  enterRegion(MBB, Begin, End, std::distance(Begin, End));
  buildSchedGraph(nullptr);

  std::vector<unsigned> InDegree(SUnits.size(), 0);
  for (const SUnit &SU : SUnits) {
    for (const SDep &Pred : SU.Preds) {
      if (Pred.getSUnit() && Pred.getSUnit()->NodeNum < SUnits.size())
        InDegree[SU.NodeNum]++;
    }
  }

  std::vector<unsigned> ReadyTime(SUnits.size(), 0);
  unsigned CurrentCycle = 0;

  auto Cmp = [&](const SUnit *A, const SUnit *B) {

    if (ReadyTime[A->NodeNum] != ReadyTime[B->NodeNum])
      return ReadyTime[A->NodeNum] > ReadyTime[B->NodeNum];

    unsigned PA = getPartition(A);
    unsigned PB = getPartition(B);
    if (PA != PB)
      return PA > PB;

    return A->NodeNum > B->NodeNum;
  };
  std::priority_queue<SUnit *, std::vector<SUnit *>, decltype(Cmp)> ReadyQ(Cmp);

  for (SUnit &SU : SUnits) {
    if (InDegree[SU.NodeNum] == 0)
      ReadyQ.push(&SU);
  }

  SmallVector<MachineInstr *, 256> NewOrder;
  while (!ReadyQ.empty()) {
    SUnit *SU = ReadyQ.top();
    ReadyQ.pop();

    if (ReadyTime[SU->NodeNum] > CurrentCycle)
      CurrentCycle = ReadyTime[SU->NodeNum];

    if (SU->getInstr())
      NewOrder.push_back(SU->getInstr());

    for (const SDep &Succ : SU->Succs) {
      SUnit *SuccSU = Succ.getSUnit();
      if (!SuccSU || SuccSU->NodeNum >= SUnits.size())
        continue;

      unsigned SuccReady = CurrentCycle + Succ.getLatency();
      if (SuccReady > ReadyTime[SuccSU->NodeNum])
        ReadyTime[SuccSU->NodeNum] = SuccReady;

      if (--InDegree[SuccSU->NodeNum] == 0)
        ReadyQ.push(SuccSU);
    }
    CurrentCycle++;
  }

  bool Changed = false;
  auto It = Begin;
  for (MachineInstr *MI : NewOrder) {
    if (&*It != MI) {
      Changed = true;
      break;
    }
    ++It;
  }

  if (!Changed) {
    exitRegion();
    finishBlock();
    return false;
  }

  for (auto I = Begin; I != End;) {
    MachineInstr &MI = *I++;
    MI.removeFromParent();
  }
  for (MachineInstr *MI : NewOrder)
    MBB->insert(End, MI);

  exitRegion();
  finishBlock();
  return true;
}

class AMDGPUPreSchedPartition : public MachineFunctionPass {
public:
  static char ID;

  AMDGPUPreSchedPartition() : MachineFunctionPass(ID) {
    initializeAMDGPUPreSchedPartitionPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "AMDGPU Pre-Scheduling Partitioner";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const GCNSubtarget *ST = nullptr;
  const SIInstrInfo *TII = nullptr;
  const SIRegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  MachineLoopInfo *MLI = nullptr;

  DenseMap<MachineInstr *, unsigned> InstrToPartition;
  DenseMap<Register, Register> TupleRoot;

  bool isTRANS(const MachineInstr &MI) const {
    return SIInstrInfo::isTRANS(MI);
  }
  bool isPKVALU(const MachineInstr &MI) const {
    return SIInstrInfo::isVOP3P(MI);
  }
  bool isWMMA(const MachineInstr &MI) const { return SIInstrInfo::isWMMA(MI); }
  bool isDSRead(const MachineInstr &MI) const {
    return SIInstrInfo::isDS(MI) && MI.mayLoad();
  }

  bool isSingleCycleVALU(const MachineInstr &MI) const {
    unsigned Opc = MI.getOpcode();
    
    if (!SIInstrInfo::isVALU(MI))
      return false;
    
    if (isWMMA(MI))
      return false;
    
    StringRef Name = TII->getName(Opc);
    if (Name.starts_with("V_CVT") || Name.starts_with("V_PERM"))
      return false;
    
    return true;
  }

  Register getKeyRegister(const MachineInstr &MI) const;

  bool isPartitionable(const MachineInstr &MI) const {
    return isTRANS(MI) || isDSRead(MI) || isWMMA(MI) ||
           (isPKVALU(MI) && !isWMMA(MI));
  }

  bool processLoopBlock(MachineBasicBlock &MBB);
  void insertSchedBarrier(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator InsertPt);
  void buildTupleMap(MachineBasicBlock &MBB);
  Register getTupleRoot(Register Reg) const;

  bool allInputsAvailableBefore(
      MachineInstr *MI, unsigned TargetPos,
      const DenseMap<MachineInstr *, unsigned> &InstrPosition);

  bool allUsesAfter(
      MachineInstr *MI, unsigned TargetPos,
      const DenseMap<MachineInstr *, unsigned> &InstrPosition);

  void redistributeTRANSAcrossPartitions(MachineBasicBlock &MBB,
                                         ArrayRef<MachineInstr *> TRANSInstrs,
                                         ArrayRef<MachineInstr *> WMMAInstrs);

  void redistributePKVALUAcrossPartitions(MachineBasicBlock &MBB,
                                          ArrayRef<MachineInstr *> PKInstrs,
                                          ArrayRef<MachineInstr *> TRANSInstrs,
                                          ArrayRef<MachineInstr *> WMMAInstrs);
};

}

char AMDGPUPreSchedPartition::ID = 0;
char &llvm::AMDGPUPreSchedPartitionID = AMDGPUPreSchedPartition::ID;

INITIALIZE_PASS_BEGIN(AMDGPUPreSchedPartition, DEBUG_TYPE,
                      "AMDGPU Pre-Scheduling Partitioner", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(AMDGPUPreSchedPartition, DEBUG_TYPE,
                    "AMDGPU Pre-Scheduling Partitioner", false, false)

Register AMDGPUPreSchedPartition::getKeyRegister(const MachineInstr &MI) const {
  if (isWMMA(MI)) {

    for (unsigned I = 0; I < MI.getNumOperands(); ++I) {
      const MachineOperand &MO = MI.getOperand(I);
      if (I == 2 && MO.isReg() && MO.getReg().isVirtual())
        return MO.getReg();
    }
  }

  for (const MachineOperand &MO : MI.defs()) {
    if (MO.isReg() && MO.getReg().isVirtual())
      return MO.getReg();
  }

  return Register();
}

void AMDGPUPreSchedPartition::insertSchedBarrier(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt) {
  DebugLoc DL;
  if (InsertPt != MBB.end())
    DL = InsertPt->getDebugLoc();
  BuildMI(MBB, InsertPt, DL, TII->get(AMDGPU::SCHED_BARRIER)).addImm(0);
}

void AMDGPUPreSchedPartition::buildTupleMap(MachineBasicBlock &MBB) {
  TupleRoot.clear();

  LLVM_DEBUG(dbgs() << "  Tuple mapping skipped (tuples already coalesced)\n");
}

Register AMDGPUPreSchedPartition::getTupleRoot(Register Reg) const {
  auto It = TupleRoot.find(Reg);
  if (It != TupleRoot.end())
    return It->second;
  return Reg;
}

bool AMDGPUPreSchedPartition::allInputsAvailableBefore(
    MachineInstr *MI, unsigned TargetPos,
    const DenseMap<MachineInstr *, unsigned> &InstrPosition) {

  for (const MachineOperand &MO : MI->uses()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;

    Register Reg = MO.getReg();

    bool hasDefInBlock = false;
    bool hasDefBeforeTarget = false;

    for (MachineInstr &DefMI : MRI->def_instructions(Reg)) {
      auto It = InstrPosition.find(&DefMI);
      if (It == InstrPosition.end())
        continue;

      hasDefInBlock = true;
      if (It->second < TargetPos)
        hasDefBeforeTarget = true;
    }

    if (hasDefInBlock && !hasDefBeforeTarget)
      return false;
  }
  return true;
}

bool AMDGPUPreSchedPartition::allUsesAfter(
    MachineInstr *MI, unsigned TargetPos,
    const DenseMap<MachineInstr *, unsigned> &InstrPosition) {

  for (const MachineOperand &MO : MI->defs()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;

    Register Reg = MO.getReg();

    for (MachineInstr &UseMI : MRI->use_instructions(Reg)) {
      auto It = InstrPosition.find(&UseMI);
      if (It == InstrPosition.end())
        continue;

      if (It->second < TargetPos)
        return false;
    }
  }
  return true;
}

// DAG for computing partition flexibility ranges without modifying code.
class PartitionPlanningDAG : public ScheduleDAGInstrs {
public:
  PartitionPlanningDAG(MachineFunction &MF, const MachineLoopInfo *MLI)
      : ScheduleDAGInstrs(MF, MLI) {}

  void schedule() override {}

  void buildDAGForRegion(MachineBasicBlock *MBB,
                         MachineBasicBlock::iterator Begin,
                         MachineBasicBlock::iterator End) {
    MachineBasicBlock::iterator ActualEnd = End;
    while (ActualEnd != Begin) {
      MachineBasicBlock::iterator Prev = std::prev(ActualEnd);
      if (!Prev->isTerminator() && !Prev->isDebugInstr())
        break;
      ActualEnd = Prev;
    }
    
    if (ActualEnd == Begin)
      return;
    
    startBlock(MBB);
    enterRegion(MBB, Begin, ActualEnd, std::distance(Begin, ActualEnd));
    buildSchedGraph(nullptr);
  }

  void finish() {
    exitRegion();
    finishBlock();
  }

  SUnit *getSUnit(MachineInstr *MI) {
    for (SUnit &SU : SUnits) {
      if (SU.getInstr() == MI)
        return &SU;
    }
    return nullptr;
  }

  bool canAssignToPartition(MachineInstr *MI, unsigned TargetPart,
                            DenseMap<MachineInstr *, unsigned> &CurrentAssignment,
                            DenseMap<MachineInstr *, unsigned> &OriginalPart,
                            bool Debug = false) {
    SUnit *SU = getSUnit(MI);
    if (!SU)
      return true;

    for (const SDep &Pred : SU->Preds) {
      SUnit *PredSU = Pred.getSUnit();
      if (!PredSU || !PredSU->getInstr())
        continue;

      MachineInstr *PredMI = PredSU->getInstr();
      unsigned PredPart = UINT_MAX;
      
      auto It = CurrentAssignment.find(PredMI);
      if (It != CurrentAssignment.end())
        PredPart = It->second;
      else {
        It = OriginalPart.find(PredMI);
        if (It != OriginalPart.end())
          PredPart = It->second;
      }

      if (PredPart != UINT_MAX && PredPart > TargetPart) {
        LLVM_DEBUG(if (Debug) dbgs() << "      Blocked by pred in P" << PredPart 
                                     << ": " << *PredMI);
        return false;
      }
    }

    for (const SDep &Succ : SU->Succs) {
      SUnit *SuccSU = Succ.getSUnit();
      if (!SuccSU || !SuccSU->getInstr())
        continue;

      MachineInstr *SuccMI = SuccSU->getInstr();
      unsigned SuccPart = UINT_MAX;
      
      auto It = CurrentAssignment.find(SuccMI);
      if (It != CurrentAssignment.end())
        SuccPart = It->second;
      else {
        It = OriginalPart.find(SuccMI);
        if (It != OriginalPart.end())
          SuccPart = It->second;
      }

      if (SuccPart != UINT_MAX && SuccPart < TargetPart) {
        LLVM_DEBUG(if (Debug) dbgs() << "      Blocked by succ in P" << SuccPart 
                                     << ": " << *SuccMI);
        return false;
      }
    }

    return true;
  }
};

// Balance TRANS instructions across partitions using DAG-based flexibility analysis.
// Computes earliest/latest possible partition for each instruction, then assigns
// to achieve roughly equal distribution.
void AMDGPUPreSchedPartition::redistributeTRANSAcrossPartitions(
    MachineBasicBlock &MBB, ArrayRef<MachineInstr *> TRANSInstrs,
    ArrayRef<MachineInstr *> WMMAInstrs) {

  if (TRANSInstrs.empty() || WMMAInstrs.size() < 49)
    return;

  DenseMap<MachineInstr *, unsigned> InstrPosition;
  DenseMap<MachineInstr *, unsigned> OriginalPart;
  unsigned Pos = 0;
  unsigned WMMACount = 0;
  unsigned WMMA17Pos = 0, WMMA33Pos = 0, WMMA49Pos = 0;

  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    InstrPosition[&*I] = Pos++;
    if (isWMMA(*I)) {
      WMMACount++;
      if (WMMACount == 17) WMMA17Pos = InstrPosition[&*I];
      if (WMMACount == 33) WMMA33Pos = InstrPosition[&*I];
      if (WMMACount == 49) WMMA49Pos = InstrPosition[&*I];
    }
  }

  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    unsigned MIPos = InstrPosition[&*I];
    if (MIPos < WMMA17Pos)
      OriginalPart[&*I] = 0;
    else if (MIPos < WMMA33Pos)
      OriginalPart[&*I] = 1;
    else if (MIPos < WMMA49Pos)
      OriginalPart[&*I] = 2;
    else
      OriginalPart[&*I] = 3;
  }

  unsigned OrigSCVALU[4] = {0, 0, 0, 0};
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    if (isSingleCycleVALU(*I))
      OrigSCVALU[OriginalPart[&*I]]++;
  }

  SmallVector<MachineInstr *, 128> OrigTRANS[4];
  for (MachineInstr *MI : TRANSInstrs) {
    OrigTRANS[OriginalPart[MI]].push_back(MI);
  }

  LLVM_DEBUG({
    dbgs() << "  TRANS distribution before: P0=" << OrigTRANS[0].size()
           << " P1=" << OrigTRANS[1].size() << " P2=" << OrigTRANS[2].size()
           << " P3=" << OrigTRANS[3].size() << "\n";
    dbgs() << "  Single-cycle VALU before: P0=" << OrigSCVALU[0]
           << " P1=" << OrigSCVALU[1] << " P2=" << OrigSCVALU[2]
           << " P3=" << OrigSCVALU[3] << "\n";
  });

  PartitionPlanningDAG DAG(*MBB.getParent(), MLI);
  DAG.buildDAGForRegion(&MBB, MBB.begin(), MBB.end());

  DenseMap<MachineInstr *, unsigned> EarliestPart, LatestPart;
  
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    if (I->isTerminator() || I->isDebugInstr())
      continue;
    EarliestPart[&*I] = 0;
    LatestPart[&*I] = 3;
  }

  // WMMA instructions are pinned: 16 per partition.
  for (unsigned I = 0; I < WMMAInstrs.size(); ++I) {
    unsigned Part = std::min(I / 16, 3u);
    EarliestPart[WMMAInstrs[I]] = Part;
    LatestPart[WMMAInstrs[I]] = Part;
    InstrToPartition[WMMAInstrs[I]] = Part;
  }

  // Propagate latest partition backward through predecessors.
  SmallVector<MachineInstr *, 256> Worklist;
  DenseSet<MachineInstr *> InWorklist;
  
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    if (I->isTerminator() || I->isDebugInstr())
      continue;
    Worklist.push_back(&*I);
    InWorklist.insert(&*I);
  }

  while (!Worklist.empty()) {
    MachineInstr *MI = Worklist.pop_back_val();
    InWorklist.erase(MI);
    
    SUnit *SU = DAG.getSUnit(MI);
    if (!SU)
      continue;
    
    unsigned MyLatest = LatestPart[MI];
    
    for (const SDep &Pred : SU->Preds) {
      SUnit *PredSU = Pred.getSUnit();
      if (!PredSU || !PredSU->getInstr())
        continue;
      MachineInstr *PredMI = PredSU->getInstr();
      
      if (LatestPart[PredMI] > MyLatest) {
        LatestPart[PredMI] = MyLatest;
        if (!InWorklist.count(PredMI)) {
          Worklist.push_back(PredMI);
          InWorklist.insert(PredMI);
        }
      }
    }
  }

  // Propagate earliest partition forward through successors.
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    if (I->isTerminator() || I->isDebugInstr())
      continue;
    Worklist.push_back(&*I);
    InWorklist.insert(&*I);
  }

  while (!Worklist.empty()) {
    MachineInstr *MI = Worklist.pop_back_val();
    InWorklist.erase(MI);
    
    SUnit *SU = DAG.getSUnit(MI);
    if (!SU)
      continue;
    
    unsigned MyEarliest = EarliestPart[MI];
    
    for (const SDep &Succ : SU->Succs) {
      SUnit *SuccSU = Succ.getSUnit();
      if (!SuccSU || !SuccSU->getInstr())
        continue;
      MachineInstr *SuccMI = SuccSU->getInstr();
      
      if (EarliestPart[SuccMI] < MyEarliest) {
        EarliestPart[SuccMI] = MyEarliest;
        if (!InWorklist.count(SuccMI)) {
          Worklist.push_back(SuccMI);
          InWorklist.insert(SuccMI);
        }
      }
    }
  }

  unsigned PinnedTRANS[4] = {0, 0, 0, 0};
  unsigned FlexTRANSByOrig[4] = {0, 0, 0, 0};
  
  for (MachineInstr *MI : TRANSInstrs) {
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    unsigned Orig = OriginalPart[MI];
    
    if (E == L) {
      PinnedTRANS[E]++;
    } else if (E <= L) {
      FlexTRANSByOrig[Orig]++;
    }
  }

  LLVM_DEBUG({
    dbgs() << "  Pinned TRANS: P0=" << PinnedTRANS[0]
           << " P1=" << PinnedTRANS[1] << " P2=" << PinnedTRANS[2]
           << " P3=" << PinnedTRANS[3] << "\n";
    dbgs() << "  Flexible TRANS by orig: P0=" << FlexTRANSByOrig[0]
           << " P1=" << FlexTRANSByOrig[1] << " P2=" << FlexTRANSByOrig[2]
           << " P3=" << FlexTRANSByOrig[3] << "\n";
  });

  if (VerboseDAGAnalysis) {
    errs() << "\n=== Verbose DAG Analysis ===\n";
    
    errs() << "TRANS flexibility:\n";
    for (MachineInstr *MI : TRANSInstrs) {
      unsigned E = EarliestPart[MI];
      unsigned L = LatestPart[MI];
      unsigned Orig = OriginalPart[MI];
      
      errs() << "  [P" << Orig << "] " << TII->getName(MI->getOpcode());
      if (MI->getNumOperands() > 0 && MI->getOperand(0).isReg())
        errs() << " %" << MI->getOperand(0).getReg().virtRegIndex();
      errs() << ": range=[" << E << "," << L << "]";
      
      if (E == L)
        errs() << " PINNED";
      else if (E < L)
        errs() << " FLEXIBLE";
      else
        errs() << " INVALID";
      errs() << "\n";
    }
    
    errs() << "\nSingle-cycle VALU flexibility:\n";
    unsigned ShownSCVALU = 0;
    for (auto I = MBB.begin(); I != MBB.end() && ShownSCVALU < 20; ++I) {
      if (!isSingleCycleVALU(*I))
        continue;
      
      MachineInstr *MI = &*I;
      unsigned E = EarliestPart[MI];
      unsigned L = LatestPart[MI];
      unsigned Orig = OriginalPart[MI];
      
      errs() << "  [P" << Orig << "] " << TII->getName(MI->getOpcode());
      if (MI->getNumOperands() > 0 && MI->getOperand(0).isReg())
        errs() << " %" << MI->getOperand(0).getReg().virtRegIndex();
      errs() << ": range=[" << E << "," << L << "]";
      
      if (E == L)
        errs() << " PINNED";
      else if (E < L)
        errs() << " FLEXIBLE";
      else
        errs() << " INVALID";
      errs() << "\n";
      ShownSCVALU++;
    }
    if (ShownSCVALU == 20)
      errs() << "  ... (truncated)\n";
    
    errs() << "=== End DAG Analysis ===\n\n";
  }

  unsigned TRANSCounts[4] = {0, 0, 0, 0};
  SmallVector<MachineInstr *, 256> FlexibleTRANS;
  
  for (MachineInstr *MI : TRANSInstrs) {
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    
    if (E == L) {
      InstrToPartition[MI] = E;
      TRANSCounts[E]++;
    } else if (E <= L) {
      FlexibleTRANS.push_back(MI);
    } else {
      unsigned P = OriginalPart[MI];
      InstrToPartition[MI] = P;
      TRANSCounts[P]++;
    }
  }

  unsigned TotalTRANS = TRANSInstrs.size();
  unsigned BaseTarget = TotalTRANS / 4;
  unsigned Remainder = TotalTRANS % 4;
  unsigned TargetTRANS[4] = {
    BaseTarget + (Remainder > 0 ? 1 : 0),
    BaseTarget + (Remainder > 1 ? 1 : 0),
    BaseTarget + (Remainder > 2 ? 1 : 0),
    BaseTarget
  };

  LLVM_DEBUG(dbgs() << "  Target TRANS: P0=" << TargetTRANS[0] 
                    << " P1=" << TargetTRANS[1]
                    << " P2=" << TargetTRANS[2] 
                    << " P3=" << TargetTRANS[3] << "\n");
  LLVM_DEBUG(dbgs() << "  Pinned TRANS: P0=" << TRANSCounts[0] 
                    << " P1=" << TRANSCounts[1]
                    << " P2=" << TRANSCounts[2] 
                    << " P3=" << TRANSCounts[3] << "\n");
  LLVM_DEBUG(dbgs() << "  Flexible TRANS: " << FlexibleTRANS.size() << "\n");

  SmallVector<MachineInstr *, 256> EdgeReachable;
  SmallVector<MachineInstr *, 256> MiddleOnly;
  
  for (MachineInstr *MI : FlexibleTRANS) {
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    
    if (E == 0 || L == 3)
      EdgeReachable.push_back(MI);
    else
      MiddleOnly.push_back(MI);
  }
  
  llvm::sort(EdgeReachable, [&](MachineInstr *A, MachineInstr *B) {
    unsigned EA = EarliestPart[A], LA = LatestPart[A];
    unsigned EB = EarliestPart[B], LB = LatestPart[B];
    
    bool ACanP0 = (EA == 0);
    bool ACanP3 = (LA == 3);
    bool BCanP0 = (EB == 0);
    bool BCanP3 = (LB == 3);
    
    bool ASingleEdge = (ACanP0 && !ACanP3) || (!ACanP0 && ACanP3);
    bool BSingleEdge = (BCanP0 && !BCanP3) || (!BCanP0 && BCanP3);
    
    if (ASingleEdge != BSingleEdge)
      return ASingleEdge;
    
    if (ASingleEdge && BSingleEdge) {
      if (ACanP0 != BCanP0)
        return ACanP0;
    }
    
    return false;
  });
  
  for (MachineInstr *MI : EdgeReachable) {
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    
    bool CanP0 = (E == 0);
    bool CanP3 = (L == 3);
    
    unsigned BestPart = E;
    int BestDeficit = INT_MIN;
    
    for (unsigned P = E; P <= L; ++P) {
      int Deficit = (int)TargetTRANS[P] - (int)TRANSCounts[P];
      
      if ((P == 0 && CanP0 && TRANSCounts[0] < TargetTRANS[0]) ||
          (P == 3 && CanP3 && TRANSCounts[3] < TargetTRANS[3])) {
        Deficit += 100;
      }
      
      if (Deficit > BestDeficit) {
        BestDeficit = Deficit;
        BestPart = P;
      }
    }
    
    InstrToPartition[MI] = BestPart;
    TRANSCounts[BestPart]++;
  }
  
  for (MachineInstr *MI : MiddleOnly) {
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    
    unsigned BestPart = E;
    int BestDeficit = INT_MIN;
    
    for (unsigned P = E; P <= L; ++P) {
      int Deficit = (int)TargetTRANS[P] - (int)TRANSCounts[P];
      if (Deficit > BestDeficit) {
        BestDeficit = Deficit;
        BestPart = P;
      }
    }
    
    InstrToPartition[MI] = BestPart;
    TRANSCounts[BestPart]++;
  }

  unsigned SCVALUCounts[4] = {0, 0, 0, 0};
  
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    if (!isSingleCycleVALU(*I))
      continue;
    
    MachineInstr *MI = &*I;
    unsigned E = EarliestPart[MI];
    unsigned L = LatestPart[MI];
    
    if (E > L) {
      unsigned P = OriginalPart[MI];
      InstrToPartition[MI] = P;
      SCVALUCounts[P]++;
      continue;
    }
    
    unsigned BestPart = E;
    int BestScore = INT_MIN;
    
    for (unsigned P = E; P <= L; ++P) {
      int Score = (int)TRANSCounts[P] - (int)SCVALUCounts[P];
      if (Score > BestScore) {
        BestScore = Score;
        BestPart = P;
      }
    }
    
    InstrToPartition[MI] = BestPart;
    SCVALUCounts[BestPart]++;
  }

  DAG.finish();

  LLVM_DEBUG({
    dbgs() << "  TRANS partition assignment: P0=" << TRANSCounts[0]
           << " P1=" << TRANSCounts[1] << " P2=" << TRANSCounts[2]
           << " P3=" << TRANSCounts[3] << "\n";
    dbgs() << "  SCVALU partition assignment: P0=" << SCVALUCounts[0]
           << " P1=" << SCVALUCounts[1] << " P2=" << SCVALUCounts[2]
           << " P3=" << SCVALUCounts[3] << "\n";
  });
}

void AMDGPUPreSchedPartition::redistributePKVALUAcrossPartitions(
    MachineBasicBlock &MBB, ArrayRef<MachineInstr *> PKInstrs,
    ArrayRef<MachineInstr *> TRANSInstrs, ArrayRef<MachineInstr *> WMMAInstrs) {

  if (PKInstrs.empty() || WMMAInstrs.size() < 49)
    return;

  DenseMap<MachineInstr *, unsigned> InstrPosition;
  DenseMap<MachineInstr *, unsigned> OriginalPart;
  unsigned Pos = 0;
  unsigned WMMACount = 0;
  unsigned WMMA17Pos = 0, WMMA33Pos = 0, WMMA49Pos = 0;

  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    InstrPosition[&*I] = Pos++;
    if (isWMMA(*I)) {
      WMMACount++;
      if (WMMACount == 17) WMMA17Pos = InstrPosition[&*I];
      if (WMMACount == 33) WMMA33Pos = InstrPosition[&*I];
      if (WMMACount == 49) WMMA49Pos = InstrPosition[&*I];
    }
  }

  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    unsigned MIPos = InstrPosition[&*I];
    if (MIPos < WMMA17Pos)
      OriginalPart[&*I] = 0;
    else if (MIPos < WMMA33Pos)
      OriginalPart[&*I] = 1;
    else if (MIPos < WMMA49Pos)
      OriginalPart[&*I] = 2;
    else
      OriginalPart[&*I] = 3;
  }

  unsigned TRANSCounts[4] = {0, 0, 0, 0};
  for (MachineInstr *MI : TRANSInstrs) {
    auto It = InstrToPartition.find(MI);
    if (It != InstrToPartition.end())
      TRANSCounts[It->second]++;
    else
      TRANSCounts[OriginalPart[MI]]++;
  }

  LLVM_DEBUG(dbgs() << "  TRANS assigned counts: P0=" << TRANSCounts[0]
                    << " P1=" << TRANSCounts[1] << " P2=" << TRANSCounts[2]
                    << " P3=" << TRANSCounts[3] << "\n");

  SmallVector<MachineInstr *, 128> OrigPK[4];

  for (MachineInstr *MI : PKInstrs) {
    OrigPK[OriginalPart[MI]].push_back(MI);
  }

  LLVM_DEBUG(dbgs() << "  V_PK counts before: P0=" << OrigPK[0].size()
                    << " P1=" << OrigPK[1].size() << " P2=" << OrigPK[2].size()
                    << " P3=" << OrigPK[3].size() << "\n");

  PartitionPlanningDAG DAG(*MBB.getParent(), MLI);
  DAG.buildDAGForRegion(&MBB, MBB.begin(), MBB.end());

  DenseMap<MachineInstr *, unsigned> EarliestPart, LatestPart;
  
  for (MachineInstr *MI : PKInstrs) {
    unsigned Earliest = 0;
    unsigned Latest = 3;
    
    SUnit *SU = DAG.getSUnit(MI);
    if (SU) {
      for (const SDep &Pred : SU->Preds) {
        SUnit *PredSU = Pred.getSUnit();
        if (!PredSU || !PredSU->getInstr())
          continue;
        auto It = InstrToPartition.find(PredSU->getInstr());
        if (It != InstrToPartition.end())
          Earliest = std::max(Earliest, It->second);
        else {
          It = OriginalPart.find(PredSU->getInstr());
          if (It != OriginalPart.end())
            Earliest = std::max(Earliest, It->second);
        }
      }
      
      for (const SDep &Succ : SU->Succs) {
        SUnit *SuccSU = Succ.getSUnit();
        if (!SuccSU || !SuccSU->getInstr())
          continue;
        auto It = InstrToPartition.find(SuccSU->getInstr());
        if (It != InstrToPartition.end())
          Latest = std::min(Latest, It->second);
        else {
          It = OriginalPart.find(SuccSU->getInstr());
          if (It != OriginalPart.end())
            Latest = std::min(Latest, It->second);
        }
      }
    }
    
    EarliestPart[MI] = Earliest;
    LatestPart[MI] = Latest;
  }

  unsigned PKCounts[4] = {0, 0, 0, 0};
  
  for (MachineInstr *MI : PKInstrs) {
    if (EarliestPart[MI] == LatestPart[MI]) {
      unsigned P = EarliestPart[MI];
      InstrToPartition[MI] = P;
      PKCounts[P]++;
    }
  }
  
  LLVM_DEBUG(dbgs() << "  Pinned V_PK: P0=" << PKCounts[0]
                    << " P1=" << PKCounts[1] << " P2=" << PKCounts[2]
                    << " P3=" << PKCounts[3] << "\n");

  for (MachineInstr *MI : PKInstrs) {
    if (EarliestPart[MI] == LatestPart[MI])
      continue;
    
    unsigned BestPart = EarliestPart[MI];
    int BestDeficit = (int)TRANSCounts[BestPart] - (int)PKCounts[BestPart];
    
    for (unsigned P = EarliestPart[MI]; P <= LatestPart[MI]; ++P) {
      int Deficit = (int)TRANSCounts[P] - (int)PKCounts[P];
      if (Deficit > BestDeficit) {
        BestDeficit = Deficit;
        BestPart = P;
      }
    }
    
    InstrToPartition[MI] = BestPart;
    PKCounts[BestPart]++;
  }

  DAG.finish();

  LLVM_DEBUG({
    dbgs() << "  V_PK assigned: P0=" << PKCounts[0]
           << " P1=" << PKCounts[1] << " P2=" << PKCounts[2]
           << " P3=" << PKCounts[3] << "\n";
  });
}

// Main entry point for processing a single loop block.
bool AMDGPUPreSchedPartition::processLoopBlock(MachineBasicBlock &MBB) {
  InstrToPartition.clear();

  buildTupleMap(MBB);

  SmallVector<MachineInstr *, 64> WMMAInstrs;
  SmallVector<MachineInstr *, 128> TRANSInstrs;
  SmallVector<MachineInstr *, 128> DSInstrs;
  SmallVector<MachineInstr *, 256> PKInstrs;

  for (MachineInstr &MI : MBB) {
    if (isWMMA(MI))
      WMMAInstrs.push_back(&MI);
    else if (isTRANS(MI))
      TRANSInstrs.push_back(&MI);
    else if (isDSRead(MI))
      DSInstrs.push_back(&MI);
    else if (isPKVALU(MI))
      PKInstrs.push_back(&MI);
  }

  unsigned TotalPartitioned = WMMAInstrs.size() + TRANSInstrs.size() +
                              DSInstrs.size() + PKInstrs.size();
  if (TotalPartitioned == 0) {
    LLVM_DEBUG(dbgs() << "  No partitionable instructions\n");
    return false;
  }

  LLVM_DEBUG(dbgs() << "  Found: " << WMMAInstrs.size() << " WMMA, "
                    << TRANSInstrs.size() << " TRANS, " << DSInstrs.size()
                    << " DS, " << PKInstrs.size() << " PK\n");

  // Assign WMMA to partitions: 16 per partition, capped at partition 3.
  for (unsigned I = 0; I < WMMAInstrs.size(); ++I) {
    unsigned Part = std::min(I / 16, 3u);
    InstrToPartition[WMMAInstrs[I]] = Part;

    if (SetPartitionHints) {
      for (const MachineOperand &MO : WMMAInstrs[I]->defs()) {
        if (MO.isReg() && MO.getReg().isVirtual()) {
          unsigned HintType = AMDGPURI::getHintForMSBBlock(Part);
          MCPhysReg RepPhys = AMDGPU::VGPR0 + (Part * 256);
          MRI->setRegAllocationHint(MO.getReg(), HintType, RepPhys);
          LLVM_DEBUG(dbgs() << "    Set WMMA MSB hint block " << Part << " on "
                            << printReg(MO.getReg(), TRI) << "\n");
          break;
        }
      }
    }
  }

  auto partitionByDest = [&](ArrayRef<MachineInstr *> Instrs) {
    if (Instrs.empty())
      return;

    DenseMap<Register, SmallVector<MachineInstr *, 8>> TupleToInstrs;
    DenseMap<Register, SmallSet<Register, 8>> TupleToComponentRegs;

    for (MachineInstr *MI : Instrs) {
      Register DestReg;
      for (const MachineOperand &MO : MI->defs()) {
        if (MO.isReg() && MO.getReg().isVirtual()) {
          DestReg = MO.getReg();
          break;
        }
      }
      if (!DestReg.isValid())
        continue;

      Register Root = getTupleRoot(DestReg);
      TupleToInstrs[Root].push_back(MI);
      TupleToComponentRegs[Root].insert(DestReg);
    }

    SmallVector<Register, 128> UniqueTuples;
    for (const auto &KV : TupleToInstrs)
      UniqueTuples.push_back(KV.first);
    llvm::sort(UniqueTuples,
               [](Register A, Register B) { return A.id() < B.id(); });

    unsigned NumTuples = UniqueTuples.size();
    unsigned ChunkSize = (NumTuples + 3) / 4;

    LLVM_DEBUG(dbgs() << "    Partitioning " << Instrs.size() << " instrs into "
                      << NumTuples << " tuple groups\n");

    for (unsigned I = 0; I < NumTuples; ++I) {
      unsigned Part = std::min(I / ChunkSize, 3u);
      Register TupleReg = UniqueTuples[I];

      for (MachineInstr *MI : TupleToInstrs[TupleReg])
        InstrToPartition[MI] = Part;

      if (SetPartitionHints) {
        for (Register CompReg : TupleToComponentRegs[TupleReg]) {
          unsigned HintType = AMDGPURI::getHintForMSBBlock(Part);
          MCPhysReg RepPhys = AMDGPU::VGPR0 + (Part * 256);
          MRI->setRegAllocationHint(CompReg, HintType, RepPhys);
        }
      }

      LLVM_DEBUG(dbgs() << "      Tuple " << printReg(TupleReg, TRI) << " -> P"
                        << Part << " (" << TupleToInstrs[TupleReg].size()
                        << " instrs, " << TupleToComponentRegs[TupleReg].size()
                        << " regs)\n");
    }
  };

  LLVM_DEBUG(dbgs() << "processLoopBlock: RedistributeTRANS=" << RedistributeTRANS 
                   << " TRANSInstrs=" << TRANSInstrs.size() 
                   << " WMMAInstrs=" << WMMAInstrs.size() << "\n");

  if (RedistributeTRANS) {
    redistributeTRANSAcrossPartitions(MBB, TRANSInstrs, WMMAInstrs);
  } else {
    partitionByDest(TRANSInstrs);
  }
  partitionByDest(DSInstrs);

  if (RedistributePKVALU) {
    redistributePKVALUAcrossPartitions(MBB, PKInstrs, TRANSInstrs, WMMAInstrs);
  } else {
    partitionByDest(PKInstrs);
  }

  if (DumpPartitionInfo) {
    unsigned TransCounts[4] = {0, 0, 0, 0};
    unsigned DSCounts[4] = {0, 0, 0, 0};
    unsigned WMMACounts[4] = {0, 0, 0, 0};
    unsigned PKCounts[4] = {0, 0, 0, 0};

    SmallVector<SmallSet<unsigned, 32>, 4> TransRegs(4);
    SmallVector<SmallSet<unsigned, 32>, 4> DSRegs(4);
    SmallVector<SmallSet<unsigned, 32>, 4> PKRegs(4);
    SmallVector<SmallSet<unsigned, 32>, 4> WMMARegs(4);
    SmallVector<SmallSet<unsigned, 32>, 4> TransTuples(4);
    SmallVector<SmallSet<unsigned, 32>, 4> DSTuples(4);
    SmallVector<SmallSet<unsigned, 32>, 4> PKTuples(4);

    for (MachineInstr *MI : WMMAInstrs) {
      unsigned P = InstrToPartition[MI];
      WMMACounts[P]++;
      for (const MachineOperand &MO : MI->defs())
        if (MO.isReg() && MO.getReg().isVirtual())
          WMMARegs[P].insert(MO.getReg().id());
    }
    for (MachineInstr *MI : TRANSInstrs) {
      unsigned P = InstrToPartition[MI];
      TransCounts[P]++;
      for (const MachineOperand &MO : MI->defs())
        if (MO.isReg() && MO.getReg().isVirtual()) {
          TransRegs[P].insert(MO.getReg().id());
          TransTuples[P].insert(getTupleRoot(MO.getReg()).id());
        }
    }
    for (MachineInstr *MI : DSInstrs) {
      unsigned P = InstrToPartition[MI];
      DSCounts[P]++;
      for (const MachineOperand &MO : MI->defs())
        if (MO.isReg() && MO.getReg().isVirtual()) {
          DSRegs[P].insert(MO.getReg().id());
          DSTuples[P].insert(getTupleRoot(MO.getReg()).id());
        }
    }
    for (MachineInstr *MI : PKInstrs) {
      unsigned P = InstrToPartition[MI];
      PKCounts[P]++;
      for (const MachineOperand &MO : MI->defs())
        if (MO.isReg() && MO.getReg().isVirtual()) {
          PKRegs[P].insert(MO.getReg().id());
          PKTuples[P].insert(getTupleRoot(MO.getReg()).id());
        }
    }

    dbgs() << "\n=== TUPLE-AWARE PARTITION (4 stripes) ===\n";
    dbgs() << "Tuple mappings: " << TupleRoot.size()
           << " component regs -> tuples\n";
    dbgs() << "Total: " << WMMAInstrs.size() << " WMMA, " << TRANSInstrs.size()
           << " TRANS, " << DSInstrs.size() << " DS, " << PKInstrs.size()
           << " PK\n\n";
    for (unsigned I = 0; I < 4; ++I) {
      unsigned Total =
          WMMACounts[I] + TransCounts[I] + DSCounts[I] + PKCounts[I];
      dbgs() << "  Partition " << I << ": " << Total << " instrs\n"
             << "    WMMA:" << WMMACounts[I] << " (" << WMMARegs[I].size()
             << " regs)\n"
             << "    TRANS:" << TransCounts[I] << " (" << TransRegs[I].size()
             << " regs, " << TransTuples[I].size() << " tuples)\n"
             << "    DS:" << DSCounts[I] << " (" << DSRegs[I].size()
             << " regs, " << DSTuples[I].size() << " tuples)\n"
             << "    PK:" << PKCounts[I] << " (" << PKRegs[I].size()
             << " regs, " << PKTuples[I].size() << " tuples)\n";
    }

    dbgs() << "\n  Register overlap analysis:\n";
    for (unsigned I = 0; I < 4; ++I) {
      for (unsigned J = I + 1; J < 4; ++J) {
        unsigned TransOverlap = 0, DSOverlap = 0, PKOverlap = 0;
        for (unsigned R : TransRegs[I])
          if (TransRegs[J].count(R))
            TransOverlap++;
        for (unsigned R : DSRegs[I])
          if (DSRegs[J].count(R))
            DSOverlap++;
        for (unsigned R : PKRegs[I])
          if (PKRegs[J].count(R))
            PKOverlap++;
        if (TransOverlap || DSOverlap || PKOverlap) {
          dbgs() << "    P" << I << "-P" << J << " overlap: "
                 << "TRANS=" << TransOverlap << " DS=" << DSOverlap
                 << " PK=" << PKOverlap << "\n";
        }
      }
    }
    dbgs() << "==========================================\n\n";
  }

  bool NeedReordering = EnableReordering || RedistributeTRANS || RedistributePKVALU;
  if (NeedReordering && !InstrToPartition.empty()) {
    PartitionDAG DAG(*MBB.getParent(), MLI, InstrToPartition);

    MachineBasicBlock::iterator Begin = MBB.begin();
    MachineBasicBlock::iterator End = MBB.end();

    while (Begin != End &&
           InstrToPartition.find(&*Begin) == InstrToPartition.end())
      ++Begin;

    while (End != Begin &&
           (std::prev(End)->isTerminator() ||
            InstrToPartition.find(&*std::prev(End)) == InstrToPartition.end()))
      --End;

    if (Begin != End) {
      LLVM_DEBUG(dbgs() << "  Reordering instructions by partition...\n");
      bool Reordered = DAG.reorderByPartition(&MBB, Begin, End);
      LLVM_DEBUG(dbgs() << "  Reorder "
                        << (Reordered ? "succeeded" : "unchanged") << "\n");
    }
  }

  // Insert SCHED_BARRIER at partition boundaries (before WMMA #17, #33, #49).
  if (InsertPartitionBarriers) {
    unsigned WMMACount = 0;
    SmallVector<MachineBasicBlock::iterator, 3> BarrierPoints;

    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end(); ++I) {
      if (I->isTerminator())
        break;

      if (isWMMA(*I)) {
        WMMACount++;

        if (WMMACount == 17 || WMMACount == 33 || WMMACount == 49) {
          BarrierPoints.push_back(I);
          LLVM_DEBUG(dbgs() << "  Barrier before WMMA #" << WMMACount
                            << " (partition " << (WMMACount / 16) << ")\n");
        }
      }
    }

    for (auto It = BarrierPoints.rbegin(); It != BarrierPoints.rend(); ++It) {
      insertSchedBarrier(MBB, *It);
    }

    LLVM_DEBUG(dbgs() << "  Inserted " << BarrierPoints.size()
                      << " SCHED_BARRIERs (expected 3)\n");
  };

  return true;
}

bool AMDGPUPreSchedPartition::runOnMachineFunction(MachineFunction &MF) {
  if (!EnablePreSchedPartition)
    return false;

  ST = &MF.getSubtarget<GCNSubtarget>();

  LLVM_DEBUG(dbgs() << "runOnMachineFunction: " << MF.getName() 
                   << " hasGFX1250Insts=" << ST->hasGFX1250Insts() << "\n");

  if (!ST->hasGFX1250Insts())
    return false;

  TII = ST->getInstrInfo();
  TRI = ST->getRegisterInfo();
  MRI = &MF.getRegInfo();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = false;

  LLVM_DEBUG(dbgs() << "=== AMDGPUPreSchedPartition on " << MF.getName()
                    << " ===\n");

  for (MachineBasicBlock &MBB : MF) {
    if (!MLI->getLoopFor(&MBB))
      continue;

    LLVM_DEBUG(dbgs() << "Processing loop BB" << MBB.getNumber() << "\n");
    Changed |= processLoopBlock(MBB);
  }

  return Changed;
}

FunctionPass *llvm::createAMDGPUPreSchedPartitionPass() {
  return new AMDGPUPreSchedPartition();
}
