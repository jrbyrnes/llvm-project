//===--- AMDGPUSchedPipeline.cpp - AMDGPU Schedule Pipelining  ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file This file contains a DAG scheduling mutation which tries to coerce
//       the scheduler into generating a specific pipeline based on types of
//       instructions.
//
//===----------------------------------------------------------------------===//

#include "AMDGPUSchedPipeline.h"
#include "AMDGPUTargetMachine.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "SIInstrInfo.h"
#include "SIMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/TargetOpcodes.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-sched-pipelining"

namespace {

static cl::opt<bool> EnableSchedPipeline(
    "amdgpu-sched-pipeline",
    cl::desc("Enable construction of pipeline for scheduling"),
    cl::init(false));

static cl::opt<unsigned> PipelineStageMaxSize(
    "amdgpu-pipeline-stage-size", cl::init(20), cl::Hidden,
    cl::desc("The maximum number of instructions to include "
             "in a pipeline stage."));

typedef function_ref<bool(const MachineInstr &)> IsInstructionType;

struct InstructionClass {
  SmallVector<SUnit *, 32> Collection;
  const IsInstructionType isInstructionClass;
  const bool IsSized;

  InstructionClass(IsInstructionType IsInstructionClass, bool isSized)
      : isInstructionClass(IsInstructionClass), IsSized(isSized){};
};

class SchedPipelineDAGMutation : public ScheduleDAGMutation {
  //  const SIInstrInfo *TII;
  //  ScheduleDAGMI *DAG;

public:
  const SIInstrInfo *TII;
  ScheduleDAGMI *DAG;

  SchedPipelineDAGMutation() = default;
  void apply(ScheduleDAGInstrs *DAGInstrs) override;
};

static void collectSUnits(SmallVectorImpl<InstructionClass *> &PipelineOrder,
                          const SIInstrInfo *TII, ScheduleDAGInstrs *DAG) {
  for (SUnit &SU : DAG->SUnits) {
    LLVM_DEBUG(dbgs() << "Checking Node"; DAG->dumpNode(SU));

    if (SU.getInstr()->getOpcode() == TargetOpcode::BUNDLE) {
      MachineInstr *MI = SU.getInstr();
      MachineBasicBlock::instr_iterator BundledMI = MI->getIterator();
      ++BundledMI;

      LLVM_DEBUG(dbgs() << "Checking bundled insts\n";);

      InstructionClass *MatchingStage = nullptr;
      for (auto Stage : PipelineOrder) {
        if (Stage->isInstructionClass(*BundledMI) &&
            (!Stage->IsSized ||
             Stage->Collection.size() < PipelineStageMaxSize)) {
          MatchingStage = Stage;
          break;
        }
      }

      if (MatchingStage != nullptr) {
        while (BundledMI->isBundledWithPred() &&
               MatchingStage->isInstructionClass(*BundledMI))
          ++BundledMI;

        if (!BundledMI->isBundledWithPred()) {
          LLVM_DEBUG(dbgs() << "Bundle is all of same type\n";);
          MatchingStage->Collection.push_back(&SU);
        }
      }
    }

    for (InstructionClass *Stage : PipelineOrder) {
      if (Stage->isInstructionClass(*SU.getInstr()) &&
          (!Stage->IsSized ||
           Stage->Collection.size() < PipelineStageMaxSize)) {
        Stage->Collection.push_back(&SU);
      }
    }
  }
}

static void
addPipelineEdges(const llvm::ArrayRef<InstructionClass *> PipelineOrder,
                 ScheduleDAGInstrs *DAG) {
  for (int i = 0; i < (int)PipelineOrder.size() - 1; i++) {
    auto StageA = PipelineOrder[i];
    for (int j = i + 1; j < (int)PipelineOrder.size(); j++) {
      auto StageB = PipelineOrder[j];

      for (auto SUnitA : StageA->Collection) {
        LLVM_DEBUG(dbgs() << "Adding edges for: "; DAG->dumpNode(*SUnitA););
        for (auto SUnitB : StageB->Collection) {
          if (DAG->canAddEdge(SUnitB, SUnitA)) {
            DAG->addEdge(SUnitB, SDep(SUnitA, SDep::Artificial));
            LLVM_DEBUG(dbgs() << "Added edge to: "; DAG->dumpNode(*SUnitB););
          } else {
            LLVM_DEBUG(dbgs() << "Can't add edge to: ";
                       DAG->dumpNode(*SUnitB););
          }
        }
      }
    }
  }
}

void SchedPipelineDAGMutation::apply(ScheduleDAGInstrs *DAGInstrs) {
  const GCNSubtarget &ST = DAGInstrs->MF.getSubtarget<GCNSubtarget>();
  TII = ST.getInstrInfo();
  if (!ST.hasMAIInsts())
    return;
  DAG = static_cast<ScheduleDAGMI *>(DAGInstrs);
  const TargetSchedModel *TSchedModel = DAGInstrs->getSchedModel();
  if (!TSchedModel || DAG->SUnits.empty())
    return;

  const IsInstructionType isMFMAFn = [this](const MachineInstr &MI) {
    if (TII->isMAI(MI) && MI.getOpcode() != AMDGPU::V_ACCVGPR_WRITE_B32_e64 &&
        MI.getOpcode() != AMDGPU::V_ACCVGPR_READ_B32_e64) {
      LLVM_DEBUG(dbgs() << "Found MFMA\n";);
      return true;
    }
    return false;
  };
  InstructionClass MFMASUnits(isMFMAFn, true);

  const IsInstructionType isVMEMReadFn = [this](const MachineInstr &MI) {
    if (((TII->isFLAT(MI) && !TII->isDS(MI)) || TII->isVMEM(MI)) &&
        MI.mayLoad()) {
      LLVM_DEBUG(dbgs() << "Found VMEM read\n";);
      return true;
    }
    return false;
  };
  InstructionClass VMEMReadSUnits(isVMEMReadFn, true);

  const IsInstructionType isDSWriteFn = [this](const MachineInstr &MI) {
    if (TII->isDS(MI) && MI.mayStore()) {
      LLVM_DEBUG(dbgs() << "Found DS Write\n";);
      return true;
    }
    return false;
  };
  InstructionClass DSWriteSUnits(isDSWriteFn, false);

  const IsInstructionType isDSReadFn = [this](const MachineInstr &MI) {
    if (TII->isDS(MI) && MI.mayLoad()) {
      LLVM_DEBUG(dbgs() << "Found DS Read\n";);
      return true;
    }
    return false;
  };
  InstructionClass DSReadSUnits(isDSReadFn, false);

  SmallVector<InstructionClass *, 4> PipelineOrder = {
      &VMEMReadSUnits, &DSReadSUnits, &MFMASUnits, &DSWriteSUnits};

  collectSUnits(PipelineOrder, TII, DAG);

  addPipelineEdges(PipelineOrder, DAG);
}

} // namespace

namespace llvm {

std::unique_ptr<ScheduleDAGMutation> createSchedPipelineDAGMutation() {
  return EnableSchedPipeline ? std::make_unique<SchedPipelineDAGMutation>()
                             : nullptr;
}

} // end namespace llvm
