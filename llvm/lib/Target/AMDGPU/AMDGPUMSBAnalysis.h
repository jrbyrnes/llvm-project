//===- AMDGPUMSBAnalysis.h - MSB State Analysis Utilities -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Shared utilities for MSB state analysis on gfx1250+. The hardware uses 2-bit
// MSB selectors to choose which 256-reg block each operand refers to.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUMSBANALYSIS_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUMSBANALYSIS_H

#include "SIDefines.h"
#include "Utils/AMDGPUBaseInfo.h" // For AMDGPU::OpName
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/MC/MCRegister.h"

namespace llvm {

class MachineRegisterInfo;
class SIInstrInfo;
class SIRegisterInfo;
class VirtRegMap;

namespace AMDGPU {

/// Per-operand MSB slot info.
struct MSBSlotOp {
  AMDGPU::OpName Name;
  unsigned FieldIdx;     // MSB field 0..3 (dst/src0/src1/src2)
  Register Reg;
  MCPhysReg Phys;
  int Block;             // VGPR block 0-3, -1 if unknown
  unsigned SizeInLanes;
  bool IsVirtual;
};

/// MSB state for one instruction (4 fields: dst, src0, src1, src2).
struct MSBFieldState {
  int Block[4] = {-1, -1, -1, -1};
  bool Present[4] = {false, false, false, false};

  bool hasConflict() const { return Conflict; }
  int getDominantBlock() const;
  unsigned countAssignedFields() const;
  bool matches(const MSBFieldState &Other) const;

private:
  friend class MSBStateAnalyzer;
  bool Conflict = false;
};

/// Analyzer for MSB state transitions.
class MSBStateAnalyzer {
public:
  MSBStateAnalyzer(const SIInstrInfo *TII, const SIRegisterInfo *TRI,
                   const VirtRegMap *VRM = nullptr,
                   const MachineRegisterInfo *MRI = nullptr)
      : TII(TII), TRI(TRI), VRM(VRM), MRI(MRI) {}

  /// Collect VGPR operands mapped to MSB slots.
  SmallVector<MSBSlotOp, 8> collectSlots(const MachineInstr &MI) const;

  /// Build field state from slot operands.
  MSBFieldState buildFieldState(ArrayRef<MSBSlotOp> Slots) const;

  /// Collect + build in one call.
  MSBFieldState getFieldState(const MachineInstr &MI) const {
    return buildFieldState(collectSlots(MI));
  }

  /// Expected MSB state after DS_READ (considers inheritance from prev instr).
  void computeExpectedStateAfterDS(const MachineInstr &DSMI,
                                   int ExpectedAfterDS[4]) const;

  /// Get VGPR block (0-3) for a register, -1 if unknown.
  int getRegisterBlock(Register R) const;

  /// Get VGPR block for physical reg (>=4 if not VGPR).
  static unsigned getPhysRegBlock(MCPhysReg Phys, const SIRegisterInfo *TRI);

  unsigned getVRegLanes(Register R) const;
  bool isDSLoad(const MachineInstr &MI) const;
  static const MachineInstr *getPrevRealInstr(const MachineInstr &MI);
  static const MachineInstr *getNextRealInstr(const MachineInstr &MI);

private:
  const SIInstrInfo *TII;
  const SIRegisterInfo *TRI;
  const VirtRegMap *VRM;
  const MachineRegisterInfo *MRI;
};

} // end namespace AMDGPU
} // end namespace llvm

#endif // LLVM_LIB_TARGET_AMDGPU_AMDGPUMSBANALYSIS_H

