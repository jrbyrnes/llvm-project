//===- SIPreAllocateWWMRegs.cpp - WWM Register Pre-allocation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Pass to pre-allocated WWM registers
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "GCNSubtarget.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "SIMachineFunctionInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "si-pre-allocate-wwm-regs"

namespace {

class SIInsertWaveBarrier : public MachineFunctionPass {
private:
  const SIInstrInfo *TII;


  std::vector<unsigned> RegsToRewrite;
#ifndef NDEBUG
  void printWWMInfo(const MachineInstr &MI);
#endif

public:
  static char ID;

  SIInsertWaveBarrier() : MachineFunctionPass(ID) {
    initializeSIInsertWaveBarrierPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {

    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:

};

} // End anonymous namespace.

INITIALIZE_PASS_BEGIN(SIInsertWaveBarrier, DEBUG_TYPE,
                "SI Pre-allocate WWM Registers", false, false)
INITIALIZE_PASS_END(SIInsertWaveBarrier, DEBUG_TYPE,
                "SI Pre-allocate WWM Registers", false, false)

char SIInsertWaveBarrier::ID = 0;

char &llvm::SIInsertWaveBarrierID = SIInsertWaveBarrier::ID;

bool SIInsertWaveBarrier::runOnMachineFunction(MachineFunction &MF) {
    return false;
    errs() << "SIInsertWaveBarrier\n";
    const GCNSubtarget &ST = MF.getSubtarget<GCNSubtarget>();
    const SIInstrInfo *TII = ST.getInstrInfo();
    bool HasIGLP = false;

    for (auto &MBB : MF) {
        for (auto &MI : MBB) {
            if (MI.getOpcode() == AMDGPU::IGLP_OPT) {
                errs() << "FOUND IGLP\n";
                HasIGLP = true;
            }
        }
        if (HasIGLP) {
            SmallVector<MachineInstr *, 128> MFMAs;
            for (auto &MI : MBB) {
                errs() << "Checking MI: "; MI.dump();
                if (TII->isMFMAorWMMA(MI)) {
                    errs() << "MFMA\n";
                    MFMAs.push_back(&MI);
                }
            }

            auto MFMACount = MFMAs.size();
            auto InstPt = MFMAs[0];
//            errs() << "MFMACount " << MFMACount << "\n";
 //           for (size_t I = MFMACount / 2; I < MFMACount; I++) {
//              auto InstPt = MFMAs[I];
//  
//              BuildMI(MBB, InstPt, InstPt->getDebugLoc(), TII->get(AMDGPU::S_BARRIER));
//            }
//            BuildMI(MBB, InstPt, InstPt->getDebugLoc(), TII->get(AMDGPU::S_BARRIER));
            InstPt = MFMAs[MFMACount/2];
            BuildMI(MBB, InstPt, InstPt->getDebugLoc(), TII->get(AMDGPU::S_BARRIER));

            
        }
        HasIGLP = false;
    }

  return true;
}
