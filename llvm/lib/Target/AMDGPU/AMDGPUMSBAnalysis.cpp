//===- AMDGPUMSBAnalysis.cpp - MSB State Analysis Utilities -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AMDGPUMSBAnalysis.h"
#include "GCNSubtarget.h"
#include "SIInstrInfo.h"
#include "SIRegisterInfo.h"
#include "Utils/AMDGPUBaseInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"

using namespace llvm;
using namespace llvm::AMDGPU;

#define DEBUG_TYPE "amdgpu-msb-analysis"

int MSBFieldState::getDominantBlock() const {
  unsigned BlockCounts[4] = {0, 0, 0, 0};
  for (unsigned F = 0; F < 4; ++F) {
    if (Present[F] && Block[F] >= 0 && Block[F] < 4)
      BlockCounts[Block[F]]++;
  }

  int Best = -1;
  unsigned BestCount = 0;
  for (unsigned B = 0; B < 4; ++B) {
    if (BlockCounts[B] > BestCount) {
      BestCount = BlockCounts[B];
      Best = static_cast<int>(B);
    }
  }
  return Best;
}

unsigned MSBFieldState::countAssignedFields() const {
  unsigned Count = 0;
  for (unsigned F = 0; F < 4; ++F)
    if (Present[F] && Block[F] >= 0)
      ++Count;
  return Count;
}

bool MSBFieldState::matches(const MSBFieldState &Other) const {
  for (unsigned F = 0; F < 4; ++F) {
    if (Present[F] && Other.Present[F] && Block[F] != Other.Block[F])
      return false;
  }
  return true;
}

unsigned MSBStateAnalyzer::getPhysRegBlock(MCPhysReg Phys,
                                            const SIRegisterInfo *TRI) {
  if (!Phys)
    return 4;

  MCPhysReg FirstSub = TRI->getSubReg(Phys, AMDGPU::sub0);
  if (!FirstSub)
    FirstSub = Phys;

  const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(FirstSub);
  if (!RC || !TRI->isVGPRClass(RC))
    return 4;

  unsigned Enc = TRI->getEncodingValue(FirstSub);
  unsigned Idx = Enc & AMDGPU::HWEncoding::REG_IDX_MASK;
  return Idx >> 8;
}

int MSBStateAnalyzer::getRegisterBlock(Register R) const {
  if (!R)
    return -1;

  MCPhysReg Phys = MCPhysReg();

  if (R.isPhysical()) {
    Phys = R;
  } else if (R.isVirtual() && VRM && VRM->hasPhys(R)) {
    Phys = VRM->getPhys(R);
  } else {
    return -1;
  }

  unsigned Block = getPhysRegBlock(Phys, TRI);
  return (Block < 4) ? static_cast<int>(Block) : -1;
}

unsigned MSBStateAnalyzer::getVRegLanes(Register R) const {
  if (!R || !R.isVirtual() || !MRI)
    return 1;

  const TargetRegisterClass *RC = MRI->getRegClass(R);
  if (!RC)
    return 1;

  unsigned Bits = TRI->getRegSizeInBits(*RC);
  unsigned Lanes = Bits / 32;
  return Lanes ? Lanes : 1;
}

bool MSBStateAnalyzer::isDSLoad(const MachineInstr &MI) const {
  if (!(MI.getDesc().TSFlags & SIInstrFlags::DS))
    return false;
  return TII->getNamedOperand(MI, OpName::vdst) != nullptr;
}

const MachineInstr *MSBStateAnalyzer::getPrevRealInstr(const MachineInstr &MI) {
  const MachineInstr *Prev = MI.getPrevNode();
  while (Prev && (Prev->isDebugInstr() || Prev->isMetaInstruction()))
    Prev = Prev->getPrevNode();
  return Prev;
}

const MachineInstr *MSBStateAnalyzer::getNextRealInstr(const MachineInstr &MI) {
  const MachineInstr *Next = MI.getNextNode();
  while (Next && (Next->isDebugInstr() || Next->isMetaInstruction()))
    Next = Next->getNextNode();
  return Next;
}

SmallVector<MSBSlotOp, 8>
MSBStateAnalyzer::collectSlots(const MachineInstr &MI) const {
  SmallVector<MSBSlotOp, 8> Slots;
  auto Tables = getVGPRLoweringOperandTables(MI.getDesc());

  auto VisitTable = [&](const OpName *Ops) {
    if (!Ops)
      return;

    for (unsigned I = 0; I < 4; ++I) {
      OpName Name = Ops[I];
      if (Name == OpName::NUM_OPERAND_NAMES)
        continue;

      const MachineOperand *Op = TII->getNamedOperand(MI, Name);
      if (!Op || !Op->isReg() || !Op->getReg())
        continue;

      Register R = Op->getReg();
      MCPhysReg Phys = MCPhysReg();
      bool IsVirtual = false;

      if (R.isPhysical()) {
        Phys = R;
      } else if (R.isVirtual()) {
        IsVirtual = true;
        if (VRM && VRM->hasPhys(R))
          Phys = VRM->getPhys(R);
      }

      // Skip non-VGPRs
      if (Phys) {
        if (getPhysRegBlock(Phys, TRI) >= 4)
          continue;
      } else if (R.isVirtual() && MRI) {
        if (!TRI->isVGPRClass(MRI->getRegClass(R)))
          continue;
      }

      int Block = -1;
      if (Phys) {
        unsigned B = getPhysRegBlock(Phys, TRI);
        Block = (B < 4) ? static_cast<int>(B) : -1;
      }

      unsigned Lanes = 1;
      if (R.isVirtual() && MRI) {
        Lanes = getVRegLanes(R);
      } else if (Phys) {
        const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Phys);
        if (RC) {
          Lanes = TRI->getRegSizeInBits(*RC) / 32;
          if (Lanes == 0)
            Lanes = 1;
        }
      }

      Slots.push_back({Name, I, R, Phys, Block, Lanes, IsVirtual});
    }
  };

  VisitTable(Tables.first);
  VisitTable(Tables.second);
  return Slots;
}

MSBFieldState
MSBStateAnalyzer::buildFieldState(ArrayRef<MSBSlotOp> Slots) const {
  MSBFieldState State;
  for (const MSBSlotOp &Op : Slots) {
    if (Op.FieldIdx >= 4)
      continue;
    unsigned F = Op.FieldIdx;
    if (!State.Present[F]) {
      State.Present[F] = true;
      State.Block[F] = Op.Block;
    } else if (State.Block[F] != Op.Block && Op.Block >= 0) {
      State.Conflict = true;
    }
  }
  return State;
}

void MSBStateAnalyzer::computeExpectedStateAfterDS(const MachineInstr &DSMI,
                                                   int ExpectedAfterDS[4]) const {
  for (unsigned F = 0; F < 4; ++F)
    ExpectedAfterDS[F] = -1;

  MSBFieldState DSState = getFieldState(DSMI);
  MSBFieldState PrevState;
  if (const MachineInstr *Prev = getPrevRealInstr(DSMI))
    PrevState = getFieldState(*Prev);

  // Fields present in DS use DS operands; others inherit from prev instr
  for (unsigned F = 0; F < 4; ++F) {
    if (DSState.Present[F])
      ExpectedAfterDS[F] = DSState.Block[F];
    else if (PrevState.Present[F])
      ExpectedAfterDS[F] = PrevState.Block[F];
  }
}

