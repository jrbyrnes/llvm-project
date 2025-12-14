//===-- AMDGPUPostRARegRewriter.cpp - Post-RA register rewriter -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tier-3 MSB stall reduction for gfx1250+. Runs post-Greedy, before
// VirtRegRewriter. Opportunistically reassigns registers to free slots in
// different blocks when DS_READ dest and next instruction operands mismatch.
// Conservative: no COPYs, no eviction by default.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUMSBAnalysis.h"
#include "GCNSubtarget.h"
#include "SIInstrInfo.h"
#include "SIRegisterInfo.h"
#include "Utils/AMDGPUBaseInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-post-ra-rewriter"

static cl::opt<bool>
    EnablePostRARewriter("amdgpu-post-ra-rewriter", cl::init(false), cl::Hidden,
                         cl::desc("Enable AMDGPU post-RA register rewriter"));

static cl::opt<bool>
    EnableMSBFixup("amdgpu-post-ra-msb-fixup", cl::init(true), cl::Hidden,
                   cl::desc("Enable MSB exposure fixup in post-RA rewriter"));

static cl::opt<bool>
    EnableBankFixup("amdgpu-post-ra-bank-fixup", cl::init(false), cl::Hidden,
                    cl::desc("Enable bank conflict fixup in post-RA rewriter"));

static cl::opt<float>
    MaxEvictionCost("amdgpu-post-ra-max-eviction-cost", cl::init(10.0f),
                    cl::Hidden,
                    cl::desc("Maximum cost for eviction in post-RA rewriter"));

static cl::opt<bool>
    EnableEviction("amdgpu-post-ra-eviction", cl::init(false), cl::Hidden,
                   cl::desc("Enable register eviction in post-RA rewriter"));

namespace {

class AMDGPUPostRARegRewriter : public MachineFunctionPass {
public:
  static char ID;

  AMDGPUPostRARegRewriter() : MachineFunctionPass(ID) {
    initializeAMDGPUPostRARegRewriterPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "AMDGPU Post-RA Register Rewriter";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addRequired<VirtRegMapWrapperLegacy>();
    AU.addRequired<LiveRegMatrixWrapperLegacy>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  LiveIntervals *LIS = nullptr;
  VirtRegMap *VRM = nullptr;
  LiveRegMatrix *Matrix = nullptr;
  MachineLoopInfo *MLI = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  const SIRegisterInfo *TRI = nullptr;
  const SIInstrInfo *TII = nullptr;
  const GCNSubtarget *ST = nullptr;
  MachineFunction *CurMF = nullptr;

  unsigned MSBFixups = 0;
  unsigned MSBUnfixable = 0;
  unsigned BankFixups = 0;
  unsigned BankUnfixable = 0;

  unsigned getBlockForPhysReg(MCPhysReg PhysReg) const {
    return TRI->getHWRegIndex(PhysReg) / 256;
  }

  unsigned getBankForPhysReg(MCPhysReg PhysReg) const {
    return TRI->getHWRegIndex(PhysReg) % 4;
  }

  bool isDSRead(const MachineInstr &MI) const {
    return SIInstrInfo::isDS(MI) && MI.mayLoad();
  }

  MachineInstr *getNextRealInstr(MachineInstr *MI) const {
    return SIInstrInfo::getNextRealInstr(MI);
  }

  Register findInterferingVReg(MCPhysReg PhysReg, const LiveInterval &LI) const;
  MCPhysReg findFreeRegInBlock(const LiveInterval &LI, unsigned Block) const;
  MCPhysReg findFreeRegExcludingBlock(const LiveInterval &LI,
                                       unsigned ExcludeBlock) const;

  struct EvictionCost {
    float SpillWeight = 0.0f;
    int MSBExposuresCreated = 0;
    int CoExecSlotsLost = 0;
    int BankConflictsCreated = 0;

    float totalCost() const {
      return SpillWeight + MSBExposuresCreated * 10.0f +
             CoExecSlotsLost * 15.0f + BankConflictsCreated * 5.0f;
    }

    bool shouldEvict() const { return totalCost() < MaxEvictionCost; }
  };

  EvictionCost computeEvictionCost(Register Victim, MCPhysReg NewHome) const;
  void doReassign(Register VReg, MCPhysReg NewPhys);
  bool tryReassignToBlock(Register VReg, unsigned TargetBlock);
  unsigned getWMMAWindowDepth(const MachineInstr &MI) const;
  bool fixMSBExposures();
  bool tryFixMSBExposure(MachineInstr &DSMI, MachineInstr &NextMI);
  bool fixBankConflicts();

  class MSBExposureAnalyzer;
  bool tryMoveTinyNextOperand(const MSBExposureAnalyzer &A, MachineInstr &NextMI);
  bool tryMoveLocalDSAddr(const MSBExposureAnalyzer &A, MachineInstr &DSMI);
};

/// Analyzes MSB state for (DS_READ -> NextMI) pairs.
class AMDGPUPostRARegRewriter::MSBExposureAnalyzer {
public:
  using SlotOp = AMDGPU::MSBSlotOp;
  using FieldState = AMDGPU::MSBFieldState;

  static bool isReassignable(const SlotOp &S) {
    return S.IsVirtual && S.Phys != MCPhysReg();
  }

  MSBExposureAnalyzer(MachineInstr &DS, MachineInstr &Next,
                      const AMDGPUPostRARegRewriter &Pass)
      : DSMI(DS), NextMI(Next), Pass(Pass),
        SharedAnalyzer(Pass.TII, Pass.TRI, Pass.VRM, Pass.MRI) {
    analyze();
  }

  bool hasConflict() const {
    return DSState.hasConflict() || NextState.hasConflict() || PrevState.hasConflict();
  }
  unsigned getDistinctBlocks() const { return DistinctBlocks; }
  ArrayRef<SlotOp> getDSSlots() const { return DSSlots; }
  ArrayRef<SlotOp> getNextSlots() const { return NextSlots; }
  int getExpectedFieldBlock(unsigned F) const {
    return (F < 4) ? ExpectedAfterDS[F] : -1;
  }

  Register getDSAddr() const { return DSAddrReg; }
  Register getDSDst() const { return DSDstReg; }
  int getDSAddrBlock() const { return DSAddrBlock; }
  int getDSDstBlock() const { return DSDstBlock; }

  int getNextFieldBlock(unsigned F) const {
    return (F < 4 && NextState.Present[F]) ? NextState.Block[F] : -1;
  }

  bool hasFreeInBlock(Register R, unsigned Block) const {
    if (Block >= 4 || !R || !R.isVirtual())
      return false;
    if (!Pass.LIS->hasInterval(R))
      return false;
    LiveInterval &LI = Pass.LIS->getInterval(R);
    return Pass.findFreeRegInBlock(LI, Block) != MCPhysReg();
  }

  bool isSingleUseIn(Register R, MachineInstr &MI) const {
    if (!R || !R.isVirtual())
      return false;
    bool SeenUse = false;
    for (MachineInstr &U : Pass.MRI->use_nodbg_instructions(R)) {
      if (&U == &MI) {
        if (SeenUse)
          return false;
        SeenUse = true;
        continue;
      }
      return false;
    }
    return SeenUse;
  }

  unsigned countDSReadsUsing(Register Addr) const {
    unsigned Cnt = 0;
    for (MachineInstr &UseMI : Pass.MRI->use_nodbg_instructions(Addr))
      if (Pass.isDSRead(UseMI))
        ++Cnt;
    return Cnt;
  }

  unsigned getVRegLanes(Register R) const {
    if (!R || !R.isVirtual())
      return 1;
    const TargetRegisterClass *RC = Pass.MRI->getRegClass(R);
    unsigned Lanes = Pass.TRI->getRegSizeInBits(*RC) / 32;
    return Lanes ? Lanes : 1;
  }

  void dumpSlots(raw_ostream &OS) const {
    auto DumpOne = [&](ArrayRef<SlotOp> Slots, StringRef Label) {
      OS << "  " << Label << ": ";
      if (Slots.empty()) {
        OS << "<none>\n";
        return;
      }
      for (const auto &S : Slots)
        OS << "f" << S.FieldIdx << "=" << printReg(S.Reg, Pass.TRI)
           << "(b" << S.Block << ") ";
      OS << "\n";
    };
    DumpOne(DSSlots, "DS");
    DumpOne(NextSlots, "Next");
  }

private:
  MachineInstr &DSMI;
  MachineInstr &NextMI;
  const AMDGPUPostRARegRewriter &Pass;
  AMDGPU::MSBStateAnalyzer SharedAnalyzer;

  SmallVector<SlotOp, 8> DSSlots, NextSlots, PrevSlots;
  FieldState DSState, NextState, PrevState;
  int ExpectedAfterDS[4] = {-1, -1, -1, -1};
  unsigned DistinctBlocks = 0;
  Register DSDstReg, DSAddrReg;
  int DSDstBlock = -1, DSAddrBlock = -1;

  void analyze() {
    DSSlots = SharedAnalyzer.collectSlots(DSMI);
    NextSlots = SharedAnalyzer.collectSlots(NextMI);
    if (const MachineInstr *Prev = AMDGPU::MSBStateAnalyzer::getPrevRealInstr(DSMI))
      PrevSlots = SharedAnalyzer.collectSlots(*Prev);

    DSState = SharedAnalyzer.buildFieldState(DSSlots);
    NextState = SharedAnalyzer.buildFieldState(NextSlots);
    PrevState = SharedAnalyzer.buildFieldState(PrevSlots);
    SharedAnalyzer.computeExpectedStateAfterDS(DSMI, ExpectedAfterDS);

    unsigned BlockCounts[4] = {0, 0, 0, 0};
    for (const auto &S : DSSlots)
      if (S.Block >= 0 && S.Block < 4)
        BlockCounts[S.Block]++;
    for (const auto &S : NextSlots)
      if (S.Block >= 0 && S.Block < 4)
        BlockCounts[S.Block]++;
    for (unsigned B = 0; B < 4; ++B)
      if (BlockCounts[B] > 0)
        ++DistinctBlocks;

    DSDstReg = extractDSDst();
    DSAddrReg = extractDSAddr();
    DSDstBlock = SharedAnalyzer.getRegisterBlock(DSDstReg);
    DSAddrBlock = SharedAnalyzer.getRegisterBlock(DSAddrReg);
  }

  Register extractDSAddr() const {
    if (MachineOperand *Op = Pass.TII->getNamedOperand(DSMI, AMDGPU::OpName::addr))
      if (Op->isReg())
        return Op->getReg();
    for (const MachineOperand &MO : DSMI.uses())
      if (MO.isReg() && MO.getReg() && Pass.TRI->isVGPR(*Pass.MRI, MO.getReg()))
        return MO.getReg();
    return Register();
  }

  Register extractDSDst() const {
    if (MachineOperand *Op = Pass.TII->getNamedOperand(DSMI, AMDGPU::OpName::vdst))
      if (Op->isReg())
        return Op->getReg();
    for (const MachineOperand &MO : DSMI.operands())
      if (MO.isReg() && MO.isDef() && MO.getReg() &&
          Pass.TRI->isVGPR(*Pass.MRI, MO.getReg()))
        return MO.getReg();
    return Register();
  }
};

} // end anonymous namespace

char AMDGPUPostRARegRewriter::ID = 0;

INITIALIZE_PASS_BEGIN(AMDGPUPostRARegRewriter, DEBUG_TYPE,
                      "AMDGPU Post-RA Register Rewriter", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(VirtRegMapWrapperLegacy)
INITIALIZE_PASS_DEPENDENCY(LiveRegMatrixWrapperLegacy)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(AMDGPUPostRARegRewriter, DEBUG_TYPE,
                    "AMDGPU Post-RA Register Rewriter", false, false)

Register AMDGPUPostRARegRewriter::findInterferingVReg(
    MCPhysReg PhysReg, const LiveInterval &LI) const {
  // Query the LiveRegMatrix for interference
  LiveIntervalUnion::Query Q(LI, Matrix->getLiveUnions()[PhysReg]);
  if (!Q.checkInterference())
    return Register();

  // Return the first interfering vreg
  for (const LiveInterval *IntfLI : Q.interferingVRegs()) {
    if (IntfLI->reg().isVirtual())
      return IntfLI->reg();
  }
  return Register();
}

MCPhysReg AMDGPUPostRARegRewriter::findFreeRegInBlock(const LiveInterval &LI,
                                                       unsigned Block) const {
  const TargetRegisterClass *RC = MRI->getRegClass(LI.reg());
  unsigned BlockStart = Block * 256;
  unsigned BlockEnd = BlockStart + 256;

  for (MCPhysReg PhysReg : RC->getRawAllocationOrder(*CurMF)) {
    unsigned HWReg = TRI->getHWRegIndex(PhysReg);
    if (HWReg < BlockStart || HWReg >= BlockEnd)
      continue;
    if (MRI->isReserved(PhysReg))
      continue;
    if (Matrix->checkInterference(LI, PhysReg) == LiveRegMatrix::IK_Free)
      return PhysReg;
  }
  return MCPhysReg();
}

MCPhysReg
AMDGPUPostRARegRewriter::findFreeRegExcludingBlock(const LiveInterval &LI,
                                                    unsigned ExcludeBlock) const {
  const TargetRegisterClass *RC = MRI->getRegClass(LI.reg());
  unsigned ExcludeStart = ExcludeBlock * 256;
  unsigned ExcludeEnd = ExcludeStart + 256;

  for (MCPhysReg PhysReg : RC->getRawAllocationOrder(*CurMF)) {
    unsigned HWReg = TRI->getHWRegIndex(PhysReg);
    if (HWReg >= ExcludeStart && HWReg < ExcludeEnd)
      continue; // Skip excluded block
    if (MRI->isReserved(PhysReg))
      continue;
    if (Matrix->checkInterference(LI, PhysReg) == LiveRegMatrix::IK_Free)
      return PhysReg;
  }
  return MCPhysReg();
}

AMDGPUPostRARegRewriter::EvictionCost
AMDGPUPostRARegRewriter::computeEvictionCost(Register Victim,
                                              MCPhysReg NewHome) const {
  EvictionCost Cost;

  if (!LIS->hasInterval(Victim))
    return Cost;

  LiveInterval &VictimLI = LIS->getInterval(Victim);
  Cost.SpillWeight = VictimLI.weight();

  MCPhysReg OldPhys = VRM->getPhys(Victim);
  unsigned OldBlock = getBlockForPhysReg(OldPhys);
  unsigned NewBlock = getBlockForPhysReg(NewHome);

  if (OldBlock == NewBlock)
    return Cost; // No block change, minimal cost

  // Check for MSB exposures created by moving victim
  for (MachineInstr &Use : MRI->use_nodbg_instructions(Victim)) {
    // Check if use is immediately after a DS_READ
    MachineInstr *Prev = Use.getPrevNode();
    while (Prev && (Prev->isDebugInstr() || Prev->isMetaInstruction()))
      Prev = Prev->getPrevNode();

    if (Prev && isDSRead(*Prev)) {
      // Get DS_READ destination block - handle both physical and virtual
      const MachineOperand &DstMO = Prev->getOperand(0);
      MCPhysReg DstPhys = MCPhysReg();

      if (DstMO.isReg()) {
        if (DstMO.getReg().isPhysical()) {
          DstPhys = DstMO.getReg();
        } else if (DstMO.getReg().isVirtual() && VRM->hasPhys(DstMO.getReg())) {
          DstPhys = VRM->getPhys(DstMO.getReg());
        }
      }

      if (DstPhys) {
        unsigned DSDstBlock = getBlockForPhysReg(DstPhys);
        // Victim was correctly placed for this pattern
        if (DSDstBlock == OldBlock && DSDstBlock != NewBlock)
          Cost.MSBExposuresCreated++;
      }
    }

    // Check for WMMA co-execution window (simplified check)
    MachineInstr *Scan = &Use;
    for (int i = 0; i < 10 && Scan; i++) {
      Scan = Scan->getPrevNode();
      while (Scan && (Scan->isDebugInstr() || Scan->isMetaInstruction()))
        Scan = Scan->getPrevNode();
      if (Scan && SIInstrInfo::isWMMA(*Scan)) {
        Cost.CoExecSlotsLost++;
        break;
      }
    }
  }

  // Check for bank conflicts created
  unsigned OldBank = getBankForPhysReg(OldPhys);
  unsigned NewBank = getBankForPhysReg(NewHome);

  if (OldBank != NewBank) {
    for (MachineInstr &Use : MRI->use_nodbg_instructions(Victim)) {
      for (const MachineOperand &MO : Use.operands()) {
        if (!MO.isReg() || MO.getReg() == Victim)
          continue;
        Register OtherReg = MO.getReg();
        MCPhysReg OtherPhys = MCPhysReg();
        if (OtherReg.isPhysical())
          OtherPhys = OtherReg;
        else if (OtherReg.isVirtual() && VRM->hasPhys(OtherReg))
          OtherPhys = VRM->getPhys(OtherReg);
        if (!OtherPhys)
          continue;

        unsigned OtherBank = getBankForPhysReg(OtherPhys);
        // Was not conflicting, now is
        if (OtherBank != OldBank && OtherBank == NewBank)
          Cost.BankConflictsCreated++;
      }
    }
  }

  return Cost;
}

void AMDGPUPostRARegRewriter::doReassign(Register VReg, MCPhysReg NewPhys) {
  if (!LIS->hasInterval(VReg))
    return;

  LiveInterval &LI = LIS->getInterval(VReg);
  MCPhysReg OldPhys = VRM->getPhys(VReg);

  LLVM_DEBUG(dbgs() << "  Reassigning " << printReg(VReg, TRI) << ": "
                    << printReg(OldPhys, TRI) << " (block "
                    << getBlockForPhysReg(OldPhys) << ") -> "
                    << printReg(NewPhys, TRI) << " (block "
                    << getBlockForPhysReg(NewPhys) << ")\n");

  // Note: Matrix->unassign/assign already update VRM internally
  Matrix->unassign(LI);
  Matrix->assign(LI, NewPhys);
}

bool AMDGPUPostRARegRewriter::tryReassignToBlock(Register VReg,
                                                  unsigned TargetBlock) {
  if (!LIS->hasInterval(VReg))
    return false;

  LiveInterval &LI = LIS->getInterval(VReg);

  // Step 1: Try to find a free register in target block
  MCPhysReg FreeReg = findFreeRegInBlock(LI, TargetBlock);
  if (FreeReg) {
    doReassign(VReg, FreeReg);
    return true;
  }

  // Step 2: Try eviction (disabled by default due to correctness concerns)
  if (!EnableEviction) {
    LLVM_DEBUG(dbgs() << "    No free reg in block " << TargetBlock
                      << ", eviction disabled\n");
    return false;
  }

  LLVM_DEBUG(dbgs() << "    No free reg in block " << TargetBlock
                    << ", trying eviction\n");

  struct EvictionCandidate {
    Register Victim;
    MCPhysReg VictimPhys;
    MCPhysReg VictimNewHome;
    EvictionCost Cost;
  };
  SmallVector<EvictionCandidate, 8> Candidates;

  const TargetRegisterClass *RC = MRI->getRegClass(VReg);
  unsigned BlockStart = TargetBlock * 256;
  unsigned BlockEnd = BlockStart + 256;

  for (MCPhysReg CandPhys : RC->getRawAllocationOrder(*CurMF)) {
    unsigned HWReg = TRI->getHWRegIndex(CandPhys);
    if (HWReg < BlockStart || HWReg >= BlockEnd)
      continue;
    if (MRI->isReserved(CandPhys))
      continue;

    // Find who's interfering
    Register Victim = findInterferingVReg(CandPhys, LI);
    if (!Victim.isValid())
      continue;

    // Can victim go somewhere else?
    if (!LIS->hasInterval(Victim))
      continue;
    LiveInterval &VictimLI = LIS->getInterval(Victim);
    MCPhysReg VictimNewHome = findFreeRegExcludingBlock(VictimLI, TargetBlock);
    if (!VictimNewHome)
      continue;

    // Compute cost
    EvictionCost Cost = computeEvictionCost(Victim, VictimNewHome);
    if (Cost.shouldEvict()) {
      Candidates.push_back({Victim, CandPhys, VictimNewHome, Cost});
    }
  }

  if (Candidates.empty()) {
    LLVM_DEBUG(dbgs() << "    No viable eviction candidate\n");
    return false;
  }

  // Pick lowest cost
  auto Best = std::min_element(
      Candidates.begin(), Candidates.end(),
      [](const EvictionCandidate &A, const EvictionCandidate &B) {
        return A.Cost.totalCost() < B.Cost.totalCost();
      });

  LLVM_DEBUG(dbgs() << "    Evicting " << printReg(Best->Victim, TRI)
                    << " (cost " << Best->Cost.totalCost() << ")\n");

  // Do the eviction: move victim, then move our vreg
  doReassign(Best->Victim, Best->VictimNewHome);
  doReassign(VReg, Best->VictimPhys);
  return true;
}

bool AMDGPUPostRARegRewriter::tryMoveTinyNextOperand(
    const MSBExposureAnalyzer &A, MachineInstr &NextMI) {
  // Move tiny (1-2 lane), single-use NextMI operand to match expected state.
  int DSDstBlock = A.getDSDstBlock();
  if (DSDstBlock < 0)
    return false;

  for (const auto &S : A.getNextSlots()) {
    if (!MSBExposureAnalyzer::isReassignable(S))
      continue;
    if (S.SizeInLanes > 2)
      continue;
    if (!A.isSingleUseIn(S.Reg, NextMI))
      continue;

    MachineInstr *DefMI = MRI->getVRegDef(S.Reg);
    if (!DefMI || DefMI->mayLoadOrStore())
      continue;

    // Target block: expected MSB field state after DS, or DS dst as fallback
    int TargetBlock = A.getExpectedFieldBlock(S.FieldIdx);
    if (TargetBlock < 0)
      TargetBlock = DSDstBlock;

    if (TargetBlock == S.Block)
      continue;

    if (!A.hasFreeInBlock(S.Reg, TargetBlock))
      continue;

    LLVM_DEBUG(dbgs() << "  Strategy A0: move NextMI operand "
                      << printReg(S.Reg, TRI) << " from block " << S.Block
                      << " -> block " << TargetBlock
                      << " (match expected f" << S.FieldIdx << ")\n");

    if (tryReassignToBlock(S.Reg, (unsigned)TargetBlock)) {
      MSBFixups++;
      return true;
    }
  }

  return false;
}

bool AMDGPUPostRARegRewriter::tryMoveLocalDSAddr(
    const MSBExposureAnalyzer &A, MachineInstr &DSMI) {
  // Move local DS addr to match NextMI's field-0 block.
  // Only if: single DS_READ user, small (<=8 lanes), free reg available.
  Register DSAddrReg = A.getDSAddr();
  int DSAddrBlock = A.getDSAddrBlock();
  int DSDstBlock = A.getDSDstBlock();

  if (!DSAddrReg || !DSAddrReg.isVirtual() || DSDstBlock < 0)
    return false;

  // DS uses MSB field 0 for addr. If NextMI uses field 0 too, then a mismatch
  // forces an s_set_vgpr_msb after the DS.
  int NextField0 = A.getNextFieldBlock(0);
  if (NextField0 < 0 || NextField0 == DSAddrBlock)
    return false;

  // Only move if exactly one DS_READ uses this address
  unsigned DSUseCount = A.countDSReadsUsing(DSAddrReg);
  if (DSUseCount != 1) {
    LLVM_DEBUG(dbgs() << "  Strategy A1 skip: addr " << printReg(DSAddrReg, TRI)
                      << " has " << DSUseCount << " DS_READ uses (not local)\n");
    return false;
  }

  // Size check
  unsigned AddrLanes = A.getVRegLanes(DSAddrReg);
  if (AddrLanes > 8)
    return false;

  // Free register check
  if (!A.hasFreeInBlock(DSAddrReg, NextField0)) {
    LLVM_DEBUG(dbgs() << "  Strategy A1 skip: no free reg for addr in block "
                      << NextField0 << "\n");
    return false;
  }

  LLVM_DEBUG(dbgs() << "  Strategy A1: move DS addr "
                    << printReg(DSAddrReg, TRI) << " from block " << DSAddrBlock
                    << " -> block " << NextField0 << " (match NextMI f0)\n");

  if (tryReassignToBlock(DSAddrReg, (unsigned)NextField0)) {
    MSBFixups++;
    return true;
  }
  return false;
}

bool AMDGPUPostRARegRewriter::tryFixMSBExposure(MachineInstr &DSMI,
                                                 MachineInstr &NextMI) {
  // Analyze the DS_READ -> NextMI transition
  MSBExposureAnalyzer A(DSMI, NextMI, *this);

  // Bail if conflicting blocks within same MSB field
  if (A.hasConflict())
    return false;

  LLVM_DEBUG({
    dbgs() << "  Analyzing DS_READ -> NextMI:\n";
    A.dumpSlots(dbgs());
  });

  // Strategy A0: Try moving a tiny, local NextMI operand
  if (tryMoveTinyNextOperand(A, NextMI))
    return true;

  // Bail if too fragmented (>2 blocks)
  if (A.getDistinctBlocks() > 2) {
    LLVM_DEBUG(dbgs() << "  -> Too fragmented (" << A.getDistinctBlocks()
                      << " blocks), skipping\n");
    MSBUnfixable++;
    return false;
  }

  // Strategy A1: Try moving a local DS address
  if (tryMoveLocalDSAddr(A, DSMI))
    return true;

  return false;
}

unsigned AMDGPUPostRARegRewriter::getWMMAWindowDepth(const MachineInstr &MI) const {
  // Scan backward up to 8 instructions to find WMMA
  const unsigned MaxScanDepth = 8;
  const MachineInstr *Scan = &MI;
  
  for (unsigned Depth = 1; Depth <= MaxScanDepth; Depth++) {
    Scan = Scan->getPrevNode();
    // Skip debug/meta instructions
    while (Scan && (Scan->isDebugInstr() || Scan->isMetaInstruction()))
      Scan = Scan->getPrevNode();
    
    if (!Scan)
      return 0;
    
    if (SIInstrInfo::isWMMA(*Scan))
      return MaxScanDepth - Depth + 1; // Higher depth = closer to WMMA = more valuable
  }
  
  return 0;
}

bool AMDGPUPostRARegRewriter::fixMSBExposures() {
  struct FixupCandidate {
    MachineInstr *DSRead;
    MachineInstr *NextMI;
    bool InLoop;
    unsigned WMMAWindowDepth;
    unsigned InstrIndex;
    
    bool operator<(const FixupCandidate &O) const {
      if (WMMAWindowDepth != O.WMMAWindowDepth)
        return WMMAWindowDepth > O.WMMAWindowDepth;
      if (InLoop != O.InLoop)
        return InLoop;
      return InstrIndex < O.InstrIndex;
    }
  };
  
  SmallVector<FixupCandidate, 64> Candidates;
  unsigned InstrIdx = 0;

  for (MachineBasicBlock &MBB : *CurMF) {
    bool IsInLoop = MLI->getLoopFor(&MBB) != nullptr;
    for (MachineInstr &MI : MBB) {
      if (!isDSRead(MI)) {
        InstrIdx++;
        continue;
      }
      MachineInstr *NextMI = getNextRealInstr(&MI);
      if (!NextMI) {
        InstrIdx++;
        continue;
      }
      Candidates.push_back({&MI, NextMI, IsInLoop, getWMMAWindowDepth(MI), InstrIdx});
      InstrIdx++;
    }
  }
  
  llvm::sort(Candidates);
  
  LLVM_DEBUG({
    unsigned InWindow = 0, InLoopOnly = 0, Other = 0;
    for (const auto &C : Candidates) {
      if (C.WMMAWindowDepth > 0) InWindow++;
      else if (C.InLoop) InLoopOnly++;
      else Other++;
    }
    dbgs() << "  Fixup candidates: " << Candidates.size() << " total\n";
    dbgs() << "    In WMMA window: " << InWindow << " (highest priority)\n";
    dbgs() << "    In loop only:   " << InLoopOnly << "\n";
    dbgs() << "    Other:          " << Other << " (lowest priority)\n";
  });

  bool Changed = false;
  for (const auto &C : Candidates) {
    Changed |= tryFixMSBExposure(*C.DSRead, *C.NextMI);
  }

  return Changed;
}

bool AMDGPUPostRARegRewriter::fixBankConflicts() {
  // TODO: Implement bank conflict fixup
  // Similar pattern: find conflicts, try reassign, evict if needed
  return false;
}

bool AMDGPUPostRARegRewriter::runOnMachineFunction(MachineFunction &MF) {
  // Must call getAnalysis BEFORE any early returns to keep pass manager happy.
  // Otherwise downstream passes may fail to find these analyses.
  LIS = &getAnalysis<LiveIntervalsWrapperPass>().getLIS();
  VRM = &getAnalysis<VirtRegMapWrapperLegacy>().getVRM();
  Matrix = &getAnalysis<LiveRegMatrixWrapperLegacy>().getLRM();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  if (!EnablePostRARewriter)
    return false;

  CurMF = &MF;
  ST = &MF.getSubtarget<GCNSubtarget>();

  // Only run on gfx1250+
  if (!ST->hasGFX1250Insts())
    return false;

  TRI = ST->getRegisterInfo();
  TII = ST->getInstrInfo();
  MRI = &MF.getRegInfo();

  LLVM_DEBUG(dbgs() << "AMDGPUPostRARegRewriter: Processing " << MF.getName()
                    << "\n");

  // Reset statistics
  MSBFixups = 0;
  MSBUnfixable = 0;
  BankFixups = 0;
  BankUnfixable = 0;

  bool Changed = false;

  if (EnableMSBFixup)
    Changed |= fixMSBExposures();

  if (EnableBankFixup)
    Changed |= fixBankConflicts();

  LLVM_DEBUG({
    dbgs() << "AMDGPUPostRARegRewriter: Summary\n";
    dbgs() << "  MSB fixups: " << MSBFixups << " fixed, " << MSBUnfixable
           << " unfixable\n";
    if (EnableBankFixup)
      dbgs() << "  Bank fixups: " << BankFixups << " fixed, " << BankUnfixable
             << " unfixable\n";
  });

  return Changed;
}

FunctionPass *llvm::createAMDGPUPostRARegRewriterPass() {
  return new AMDGPUPostRARegRewriter();
}
