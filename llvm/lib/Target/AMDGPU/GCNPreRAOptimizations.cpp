//===-- GCNPreRAOptimizations.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This pass combines split register tuple initialization into a single pseudo:
///
///   undef %0.sub1:sreg_64 = S_MOV_B32 1
///   %0.sub0:sreg_64 = S_MOV_B32 2
/// =>
///   %0:sreg_64 = S_MOV_B64_IMM_PSEUDO 0x200000001
///
/// This is to allow rematerialization of a value instead of spilling. It is
/// supposed to be done after register coalescer to allow it to do its job and
/// before actual register allocation to allow rematerialization.
///
/// Right now the pass only handles 64 bit SGPRs with immediate initializers,
/// although the same shall be possible with other register classes and
/// instructions if necessary.
///
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "GCNSubtarget.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-pre-ra-optimizations"

namespace {

class GCNPreRAOptimizations : public MachineFunctionPass {
private:
  const SIInstrInfo *TII;
  const SIRegisterInfo *TRI;
  MachineRegisterInfo *MRI;
  LiveIntervals *LIS;

  bool processReg(Register Reg);

    // Check if the machine instruction being processed is a supported packed
  // instruction.
  bool isUnpackingSupportedInstr(MachineInstr &MI) const;
  // Creates a list of packed instructions following an MFMA that are suitable
  // for unpacking.
  void collectUnpackingCandidates(MachineInstr &BeginMI,
                                  SetVector<MachineInstr *> &InstrsToUnpack,
                                  uint16_t NumMFMACycles);
  // v_pk_fma_f32 v[0:1], v[0:1], v[2:3], v[2:3] op_sel:[1,1,1]
  // op_sel_hi:[0,0,0]
  // ==>
  // v_fma_f32 v0, v1, v3, v3
  // v_fma_f32 v1, v0, v2, v2
  // Here, we have overwritten v0 before we use it. This function checks if
  // unpacking can lead to such a situation.
  bool canUnpackingClobberRegister(const MachineInstr &MI);
  // Unpack and insert F32 packed instructions, such as V_PK_MUL, V_PK_ADD, and
  // V_PK_FMA. Currently, only V_PK_MUL, V_PK_ADD, V_PK_FMA are supported for
  // this transformation.
  void performF32Unpacking(MachineInstr &I);
  // Select corresponding unpacked instruction
  uint16_t mapToUnpackedOpcode(MachineInstr &I);
  // Creates the unpacked instruction to be inserted. Adds source modifiers to
  // the unpacked instructions based on the source modifiers in the packed
  // instruction.
  MachineInstrBuilder createUnpackedMI(MachineInstr &I, uint16_t UnpackedOpcode,
                                       bool IsHiBits);
  // Process operands/source modifiers from packed instructions and insert the
  // appropriate source modifers and operands into the unpacked instructions.
  void addOperandAndMods(MachineInstrBuilder &NewMI, unsigned SrcMods,
                         bool IsHiBits, const MachineOperand &SrcMO);

public:
  static char ID;

  GCNPreRAOptimizations() : MachineFunctionPass(ID) {
    initializeGCNPreRAOptimizationsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "AMDGPU Pre-RA optimizations";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // End anonymous namespace.

INITIALIZE_PASS_BEGIN(GCNPreRAOptimizations, DEBUG_TYPE,
                      "AMDGPU Pre-RA optimizations", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_END(GCNPreRAOptimizations, DEBUG_TYPE, "Pre-RA optimizations",
                    false, false)

char GCNPreRAOptimizations::ID = 0;

char &llvm::GCNPreRAOptimizationsID = GCNPreRAOptimizations::ID;

FunctionPass *llvm::createGCNPreRAOptimizationsPass() {
  return new GCNPreRAOptimizations();
}

bool GCNPreRAOptimizations::processReg(Register Reg) {
  MachineInstr *Def0 = nullptr;
  MachineInstr *Def1 = nullptr;
  uint64_t Init = 0;
  bool Changed = false;
  SmallSet<Register, 32> ModifiedRegs;
  bool IsAGPRDst = TRI->isAGPRClass(MRI->getRegClass(Reg));

  for (MachineInstr &I : MRI->def_instructions(Reg)) {
    switch (I.getOpcode()) {
    default:
      return false;
    case AMDGPU::V_ACCVGPR_WRITE_B32_e64:
      break;
    case AMDGPU::COPY: {
      // Some subtargets cannot do an AGPR to AGPR copy directly, and need an
      // intermdiate temporary VGPR register. Try to find the defining
      // accvgpr_write to avoid temporary registers.

      if (!IsAGPRDst)
        return false;

      Register SrcReg = I.getOperand(1).getReg();

      if (!SrcReg.isVirtual())
        break;

      // Check if source of copy is from another AGPR.
      bool IsAGPRSrc = TRI->isAGPRClass(MRI->getRegClass(SrcReg));
      if (!IsAGPRSrc)
        break;

      // def_instructions() does not look at subregs so it may give us a
      // different instruction that defines the same vreg but different subreg
      // so we have to manually check subreg.
      Register SrcSubReg = I.getOperand(1).getSubReg();
      for (auto &Def : MRI->def_instructions(SrcReg)) {
        if (SrcSubReg != Def.getOperand(0).getSubReg())
          continue;

        if (Def.getOpcode() == AMDGPU::V_ACCVGPR_WRITE_B32_e64) {
          MachineOperand DefSrcMO = Def.getOperand(1);

          // Immediates are not an issue and can be propagated in
          // postrapseudos pass. Only handle cases where defining
          // accvgpr_write source is a vreg.
          if (DefSrcMO.isReg() && DefSrcMO.getReg().isVirtual()) {
            // Propagate source reg of accvgpr write to this copy instruction
            I.getOperand(1).setReg(DefSrcMO.getReg());
            I.getOperand(1).setSubReg(DefSrcMO.getSubReg());

            // Reg uses were changed, collect unique set of registers to update
            // live intervals at the end.
            ModifiedRegs.insert(DefSrcMO.getReg());
            ModifiedRegs.insert(SrcReg);

            Changed = true;
          }

          // Found the defining accvgpr_write, stop looking any further.
          break;
        }
      }
      break;
    }
    case AMDGPU::S_MOV_B32:
      if (I.getOperand(0).getReg() != Reg || !I.getOperand(1).isImm() ||
          I.getNumOperands() != 2)
        return false;

      switch (I.getOperand(0).getSubReg()) {
      default:
        return false;
      case AMDGPU::sub0:
        if (Def0)
          return false;
        Def0 = &I;
        Init |= Lo_32(I.getOperand(1).getImm());
        break;
      case AMDGPU::sub1:
        if (Def1)
          return false;
        Def1 = &I;
        Init |= static_cast<uint64_t>(I.getOperand(1).getImm()) << 32;
        break;
      }
      break;
    }
  }

  // For AGPR reg, check if live intervals need to be updated.
  if (IsAGPRDst) {
    if (Changed) {
      for (Register RegToUpdate : ModifiedRegs) {
        LIS->removeInterval(RegToUpdate);
        LIS->createAndComputeVirtRegInterval(RegToUpdate);
      }
    }

    return Changed;
  }

  // For SGPR reg, check if we can combine instructions.
  if (!Def0 || !Def1 || Def0->getParent() != Def1->getParent())
    return Changed;

  LLVM_DEBUG(dbgs() << "Combining:\n  " << *Def0 << "  " << *Def1
                    << "    =>\n");

  if (SlotIndex::isEarlierInstr(LIS->getInstructionIndex(*Def1),
                                LIS->getInstructionIndex(*Def0)))
    std::swap(Def0, Def1);

  LIS->RemoveMachineInstrFromMaps(*Def0);
  LIS->RemoveMachineInstrFromMaps(*Def1);
  auto NewI = BuildMI(*Def0->getParent(), *Def0, Def0->getDebugLoc(),
                      TII->get(AMDGPU::S_MOV_B64_IMM_PSEUDO), Reg)
                  .addImm(Init);

  Def0->eraseFromParent();
  Def1->eraseFromParent();
  LIS->InsertMachineInstrInMaps(*NewI);
  LIS->removeInterval(Reg);
  LIS->createAndComputeVirtRegInterval(Reg);

  LLVM_DEBUG(dbgs() << "  " << *NewI);

  return true;
}


// If support is extended to new operations, add tests in
// llvm/test/CodeGen/AMDGPU/unpack-non-coissue-insts-post-ra-scheduler.mir.
bool GCNPreRAOptimizations::isUnpackingSupportedInstr(MachineInstr &MI) const {
  if (!TII->isNeverCoissue(MI))
    return false;
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  case AMDGPU::V_PK_ADD_F32:
  case AMDGPU::V_PK_MUL_F32:
  case AMDGPU::V_PK_FMA_F32:
    return true;
  default:
    return false;
  }
  llvm_unreachable("Fully covered switch");
}


bool GCNPreRAOptimizations::canUnpackingClobberRegister(const MachineInstr &MI) {
  unsigned OpCode = MI.getOpcode();
  Register DstReg = MI.getOperand(0).getReg();
  // Only the first register in the register pair needs to be checked due to the
  // unpacking order. Packed instructions are unpacked such that the lower 32
  // bits (i.e., the first register in the pair) are written first. This can
  // introduce dependencies if the first register is written in one instruction
  // and then read as part of the higher 32 bits in the subsequent instruction.
  // Such scenarios can arise due to specific combinations of op_sel and
  // op_sel_hi modifiers.
  Register UnpackedDstReg = TRI->getSubReg(DstReg, AMDGPU::sub0);

  const MachineOperand *Src0MO = TII->getNamedOperand(MI, AMDGPU::OpName::src0);
  if (Src0MO && Src0MO->isReg()) {
    Register SrcReg0 = Src0MO->getReg();
    unsigned Src0Mods =
        TII->getNamedOperand(MI, AMDGPU::OpName::src0_modifiers)->getImm();
    Register HiSrc0Reg = (Src0Mods & SISrcMods::OP_SEL_1)
                             ? TRI->getSubReg(SrcReg0, AMDGPU::sub1)
                             : TRI->getSubReg(SrcReg0, AMDGPU::sub0);
    // Check if the register selected by op_sel_hi is the same as the first
    // register in the destination register pair.
    if (TRI->regsOverlap(UnpackedDstReg, HiSrc0Reg))
      return true;
  }

  const MachineOperand *Src1MO = TII->getNamedOperand(MI, AMDGPU::OpName::src1);
  if (Src1MO && Src1MO->isReg()) {
    Register SrcReg1 = Src1MO->getReg();
    unsigned Src1Mods =
        TII->getNamedOperand(MI, AMDGPU::OpName::src1_modifiers)->getImm();
    Register HiSrc1Reg = (Src1Mods & SISrcMods::OP_SEL_1)
                             ? TRI->getSubReg(SrcReg1, AMDGPU::sub1)
                             : TRI->getSubReg(SrcReg1, AMDGPU::sub0);
    if (TRI->regsOverlap(UnpackedDstReg, HiSrc1Reg))
      return true;
  }

  // Applicable for packed instructions with 3 source operands, such as
  // V_PK_FMA.
  if (AMDGPU::hasNamedOperand(OpCode, AMDGPU::OpName::src2)) {
    const MachineOperand *Src2MO =
        TII->getNamedOperand(MI, AMDGPU::OpName::src2);
    if (Src2MO && Src2MO->isReg()) {
      Register SrcReg2 = Src2MO->getReg();
      unsigned Src2Mods =
          TII->getNamedOperand(MI, AMDGPU::OpName::src2_modifiers)->getImm();
      Register HiSrc2Reg = (Src2Mods & SISrcMods::OP_SEL_1)
                               ? TRI->getSubReg(SrcReg2, AMDGPU::sub1)
                               : TRI->getSubReg(SrcReg2, AMDGPU::sub0);
      if (TRI->regsOverlap(UnpackedDstReg, HiSrc2Reg))
        return true;
    }
  }
  return false;
}

uint16_t GCNPreRAOptimizations::mapToUnpackedOpcode(MachineInstr &I) {
  unsigned Opcode = I.getOpcode();
  // Use 64 bit encoding to allow use of VOP3 instructions.
  // VOP3 e64 instructions allow source modifiers
  // e32 instructions don't allow source modifiers.
  switch (Opcode) {
  case AMDGPU::V_PK_ADD_F32:
    return AMDGPU::V_ADD_F32_e64;
  case AMDGPU::V_PK_MUL_F32:
    return AMDGPU::V_MUL_F32_e64;
  case AMDGPU::V_PK_FMA_F32:
    return AMDGPU::V_FMA_F32_e64;
  default:
    return std::numeric_limits<uint16_t>::max();
  }
  llvm_unreachable("Fully covered switch");
}

void GCNPreRAOptimizations::addOperandAndMods(MachineInstrBuilder &NewMI,
                                          unsigned SrcMods, bool IsHiBits,
                                          const MachineOperand &SrcMO) {
  unsigned NewSrcMods = 0;
  unsigned NegModifier = IsHiBits ? SISrcMods::NEG_HI : SISrcMods::NEG;
  unsigned OpSelModifier = IsHiBits ? SISrcMods::OP_SEL_1 : SISrcMods::OP_SEL_0;
  // Packed instructions (VOP3P) do not support ABS. Hence, no checks are done
  // for ABS modifiers.
  // If NEG or NEG_HI is true, we need to negate the corresponding 32 bit
  // lane.
  // NEG_HI shares the same bit position with ABS. But packed instructions do
  // not support ABS. Therefore, NEG_HI must be translated to NEG source
  // modifier for the higher 32 bits. Unpacked VOP3 instructions support
  // ABS, but do not support NEG_HI. Therefore we need to explicitly add the
  // NEG modifier if present in the packed instruction.
  if (SrcMods & NegModifier)
    NewSrcMods |= SISrcMods::NEG;
  // Src modifiers. Only negative modifiers are added if needed. Unpacked
  // operations do not have op_sel, therefore it must be handled explicitly as
  // done below.
  NewMI.addImm(NewSrcMods);
  if (SrcMO.isImm()) {
    NewMI.addImm(SrcMO.getImm());
    return;
  }
  // If op_sel == 0, select register 0 of reg:sub0_sub1.
  Register UnpackedSrcReg = (SrcMods & OpSelModifier)
                                ? TRI->getSubReg(SrcMO.getReg(), AMDGPU::sub1)
                                : TRI->getSubReg(SrcMO.getReg(), AMDGPU::sub0);

  MachineOperand UnpackedSrcMO =
      MachineOperand::CreateReg(UnpackedSrcReg, /*isDef=*/false);
  if (SrcMO.isKill()) {
    // For each unpacked instruction, mark its source registers as killed if the
    // corresponding source register in the original packed instruction was
    // marked as killed.
    //
    // Exception:
    // If the op_sel and op_sel_hi modifiers require both unpacked instructions
    // to use the same register (e.g., due to overlapping access to low/high
    // bits of the same packed register), then only the *second* (latter)
    // instruction should mark the register as killed. This is because the
    // second instruction handles the higher bits and is effectively the last
    // user of the full register pair.

    bool OpSel = SrcMods & SISrcMods::OP_SEL_0;
    bool OpSelHi = SrcMods & SISrcMods::OP_SEL_1;
    bool KillState = true;
    if ((OpSel == OpSelHi) && !IsHiBits)
      KillState = false;
    UnpackedSrcMO.setIsKill(KillState);
  }
  NewMI.add(UnpackedSrcMO);
}

void GCNPreRAOptimizations::collectUnpackingCandidates(
    MachineInstr &BeginMI, SetVector<MachineInstr *> &InstrsToUnpack,
    uint16_t NumMFMACycles) {
  auto *BB = BeginMI.getParent();
  auto E = BB->end();
  int TotalCyclesBetweenCandidates = 0;
  auto SchedModel = TII->getSchedModel();
  Register MFMADef = BeginMI.getOperand(0).getReg();

  for (auto I = std::next(BeginMI.getIterator()); I != E; ++I) {
    MachineInstr &Instr = *I;
    if (Instr.isMetaInstruction())
      continue;
    if ((Instr.isTerminator()) ||
         (SIInstrInfo::modifiesModeRegister(Instr) &&
         Instr.modifiesRegister(AMDGPU::EXEC, TRI)))
      return;

    const MCSchedClassDesc *InstrSchedClassDesc =
        SchedModel.resolveSchedClass(&Instr);
    uint16_t Latency =
        SchedModel.getWriteProcResBegin(InstrSchedClassDesc)->ReleaseAtCycle;
    TotalCyclesBetweenCandidates += Latency;

    if (TotalCyclesBetweenCandidates >= NumMFMACycles - 1)
      return;
    // Identify register dependencies between those used by the MFMA
    // instruction and the following packed instructions. Also checks for
    // transitive dependencies between the MFMA def and candidate instruction
    // def and uses. Conservatively ensures that we do not incorrectly
    // read/write registers.
    for (const MachineOperand &InstrMO : Instr.operands()) {
      if (!InstrMO.isReg() || !InstrMO.getReg().isValid())
        continue;
      if (TRI->regsOverlap(MFMADef, InstrMO.getReg()))
        return;
    }
    if (!isUnpackingSupportedInstr(Instr))
      continue;

    if (canUnpackingClobberRegister(Instr))
      return;
    // If it's a packed instruction, adjust latency: remove the packed
    // latency, add latency of two unpacked instructions (currently estimated
    // as 2 cycles).
    TotalCyclesBetweenCandidates -= Latency;
    // TODO: improve latency handling based on instruction modeling.
    TotalCyclesBetweenCandidates += 2;
    // Subtract 1 to account for MFMA issue latency.
    if (TotalCyclesBetweenCandidates < NumMFMACycles - 1)
      InstrsToUnpack.insert(&Instr);
  }
  return;
}

void GCNPreRAOptimizations::performF32Unpacking(MachineInstr &I) {
  MachineOperand DstOp = I.getOperand(0);

  uint16_t UnpackedOpcode = mapToUnpackedOpcode(I);
  assert(UnpackedOpcode != std::numeric_limits<uint16_t>::max() &&
         "Unsupported Opcode");

  MachineInstrBuilder Op0LOp1L =
      createUnpackedMI(I, UnpackedOpcode, /*IsHiBits=*/false);
  MachineOperand LoDstOp = Op0LOp1L->getOperand(0);

  LoDstOp.setIsUndef(DstOp.isUndef());

  MachineInstrBuilder Op0HOp1H =
      createUnpackedMI(I, UnpackedOpcode, /*IsHiBits=*/true);
  MachineOperand HiDstOp = Op0HOp1H->getOperand(0);

  if (I.getFlag(MachineInstr::MIFlag::NoFPExcept)) {
    Op0LOp1L->setFlag(MachineInstr::MIFlag::NoFPExcept);
    Op0HOp1H->setFlag(MachineInstr::MIFlag::NoFPExcept);
  }
  if (I.getFlag(MachineInstr::MIFlag::FmContract)) {
    Op0LOp1L->setFlag(MachineInstr::MIFlag::FmContract);
    Op0HOp1H->setFlag(MachineInstr::MIFlag::FmContract);
  }

  LoDstOp.setIsRenamable(DstOp.isRenamable());
  HiDstOp.setIsRenamable(DstOp.isRenamable());

  I.eraseFromParent();
  return;
}

MachineInstrBuilder GCNPreRAOptimizations::createUnpackedMI(MachineInstr &I,
                                                        uint16_t UnpackedOpcode,
                                                        bool IsHiBits) {
  MachineBasicBlock &MBB = *I.getParent();
  const DebugLoc &DL = I.getDebugLoc();
  const MachineOperand *SrcMO1 = TII->getNamedOperand(I, AMDGPU::OpName::src0);
  const MachineOperand *SrcMO2 = TII->getNamedOperand(I, AMDGPU::OpName::src1);
  Register DstReg = I.getOperand(0).getReg();
  unsigned OpCode = I.getOpcode();
  Register UnpackedDstReg = IsHiBits ? TRI->getSubReg(DstReg, AMDGPU::sub1)
                                     : TRI->getSubReg(DstReg, AMDGPU::sub0);

  int64_t ClampVal = TII->getNamedOperand(I, AMDGPU::OpName::clamp)->getImm();
  unsigned Src0Mods =
      TII->getNamedOperand(I, AMDGPU::OpName::src0_modifiers)->getImm();
  unsigned Src1Mods =
      TII->getNamedOperand(I, AMDGPU::OpName::src1_modifiers)->getImm();

  MachineInstrBuilder NewMI = BuildMI(MBB, I, DL, TII->get(UnpackedOpcode));
  NewMI.addDef(UnpackedDstReg); // vdst
  addOperandAndMods(NewMI, Src0Mods, IsHiBits, *SrcMO1);
  addOperandAndMods(NewMI, Src1Mods, IsHiBits, *SrcMO2);

  if (AMDGPU::hasNamedOperand(OpCode, AMDGPU::OpName::src2)) {
    const MachineOperand *SrcMO3 =
        TII->getNamedOperand(I, AMDGPU::OpName::src2);
    unsigned Src2Mods =
        TII->getNamedOperand(I, AMDGPU::OpName::src2_modifiers)->getImm();
    addOperandAndMods(NewMI, Src2Mods, IsHiBits, *SrcMO3);
  }
  NewMI.addImm(ClampVal); // clamp
  // Packed instructions do not support output modifiers. safe to assign them 0
  // for this use case
  NewMI.addImm(0); // omod
  return NewMI;
}


bool GCNPreRAOptimizations::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  const GCNSubtarget &ST = MF.getSubtarget<GCNSubtarget>();
  TII = ST.getInstrInfo();
  MRI = &MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervalsWrapperPass>().getLIS();
  TRI = ST.getRegisterInfo();

  bool Changed = false;

  for (unsigned I = 0, E = MRI->getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (!LIS->hasInterval(Reg))
      continue;
    const TargetRegisterClass *RC = MRI->getRegClass(Reg);
    if ((RC->MC->getSizeInBits() != 64 || !TRI->isSGPRClass(RC)) &&
        (ST.hasGFX90AInsts() || !TRI->isAGPRClass(RC)))
      continue;

    Changed |= processReg(Reg);
  }

    // TODO: Fold this into previous block, if possible. Evaluate and handle any
  // side effects.
  for (MachineBasicBlock &MBB : MF) {
    // Unpack packed instructions overlapped by MFMAs. This allows the compiler
    // to co-issue unpacked instructions with MFMA
    auto SchedModel = TII->getSchedModel();
    SetVector<MachineInstr *> InstrsToUnpack;
    for (auto &MI : make_early_inc_range(MBB.instrs())) {
      if (!SIInstrInfo::isMFMA(MI))
        continue;
      const MCSchedClassDesc *SchedClassDesc =
          SchedModel.resolveSchedClass(&MI);
      uint16_t NumMFMACycles =
          SchedModel.getWriteProcResBegin(SchedClassDesc)->ReleaseAtCycle;
      collectUnpackingCandidates(MI, InstrsToUnpack, NumMFMACycles);
    }
    for (MachineInstr *MI : InstrsToUnpack) {
      performF32Unpacking(*MI);
    }
  }

  return Changed;
}
