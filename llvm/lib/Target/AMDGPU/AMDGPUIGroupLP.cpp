//===--- AMDGPUIGroupLP.cpp - AMDGPU IGroupLP  ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file This file defines a set of schedule DAG mutations that can be used to
// override default scheduler behavior to enforce specific scheduling patterns.
// They should be used in cases where runtime performance considerations such as
// inter-wavefront interactions, mean that compile-time heuristics cannot
// predict the optimal instruction ordering, or in kernels where optimum
// instruction scheduling is important enough to warrant manual intervention.
//
//===----------------------------------------------------------------------===//

#include "AMDGPUIGroupLP.h"
#include "AMDGPUTargetMachine.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "SIInstrInfo.h"
#include "SIMachineFunctionInfo.h"
#include "llvm/ADT/BitmaskEnum.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include <algorithm>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "machine-scheduler"

namespace {

static cl::opt<bool>
    EnableIGroupLP("amdgpu-igrouplp",
                   cl::desc("Enable construction of Instruction Groups and "
                            "their ordering for scheduling"),
                   cl::init(false));

enum class SchedGroupMask {
  NONE = 0u,
  ALU = 1u << 0,
  VALU = 1u << 1,
  SALU = 1u << 2,
  MFMA = 1u << 3,
  VMEM = 1u << 4,
  VMEM_READ = 1u << 5,
  VMEM_WRITE = 1u << 6,
  DS = 1u << 7,
  DS_READ = 1u << 8,
  DS_WRITE = 1u << 9,
  ALL = ALU | VALU | SALU | MFMA | VMEM | VMEM_READ | VMEM_WRITE | DS |
        DS_READ | DS_WRITE,
  LLVM_MARK_AS_BITMASK_ENUM(/* LargestFlag = */ ALL)
};

// The order of IGroup stages and their optional sizes as returned
// by the parser.
static SmallVector<std::pair<SchedGroupMask, Optional<unsigned>>, 8>
    IGroupLPOrder;

struct IGroupOrderParser
    : public cl::parser<std::pair<SchedGroupMask, Optional<unsigned>>> {
  IGroupOrderParser(cl::Option &O)
      : cl::parser<std::pair<SchedGroupMask, Optional<unsigned>>>(O) {}

  // Possible categories that a comma seperated string may
  // fall into
  enum Token { tok_start, tok_group, tok_number, tok_error };

  // The previously encountered token type
  unsigned PrevToken = tok_start;

  // A bit vector encoding the encountered groups thus far
  SchedGroupMask ObservedGroups = SchedGroupMask::NONE;

  SchedGroupMask getMaskFromStr(const std::string &Token) {
    if (Token == "alu")
      return SchedGroupMask::ALU;
    else if (Token == "valu")
      return SchedGroupMask::VALU;
    else if (Token == "salu")
      return SchedGroupMask::SALU;
    else if (Token == "mfma")
      return SchedGroupMask::MFMA;
    else if (Token == "vmem")
      return SchedGroupMask::VMEM;
    else if (Token == "vmemr")
      return SchedGroupMask::VMEM_READ;
    else if (Token == "vmemw")
      return SchedGroupMask::VMEM_WRITE;
    else if (Token == "ds")
      return SchedGroupMask::DS;
    else if (Token == "dsr")
      return SchedGroupMask::DS_READ;
    else if (Token == "dsw")
      return SchedGroupMask::DS_WRITE;

    else
      return SchedGroupMask::NONE;
  }

  // Ensure that we do not have multiple occurances of the same
  // igroup (including potential SubGroups).
  unsigned handleGroup(std::string &Token, cl::Option &O,
                       SchedGroupMask TokenMask) {
    assert(TokenMask != SchedGroupMask::NONE &&
           TokenMask != SchedGroupMask::ALL);
    if ((ObservedGroups & TokenMask) != SchedGroupMask::NONE) {
      O.error("Multiple occurance of " + Token);
      return tok_error;
    }

    SchedGroupMask SubGroupMask = SchedGroupMask::NONE;
    if (TokenMask == SchedGroupMask::ALU)
      SubGroupMask =
          SchedGroupMask::SALU | SchedGroupMask::VALU | SchedGroupMask::MFMA;
    else if (TokenMask == SchedGroupMask::DS)
      SubGroupMask = SchedGroupMask::DS_READ | SchedGroupMask::DS_WRITE;
    else if (TokenMask == SchedGroupMask::VMEM)
      SubGroupMask = SchedGroupMask::VMEM_READ | SchedGroupMask::VMEM_WRITE;

    if (SubGroupMask != SchedGroupMask::NONE) {
      if ((ObservedGroups & SubGroupMask) != SchedGroupMask::NONE) {
        O.error("Multiple occurance " + Token +
                ". Overlaps with existing SubGroup");
        return tok_error;
      }
    }

    // Add group token to encountered groups
    ObservedGroups |= TokenMask;
    // Add sub group token to encountered groups
    ObservedGroups |= SubGroupMask;

    return tok_group;
  }

  // Check for properly formatted numbers and igroup strings.
  // If we are unable to easily find one, then flag as error.
  unsigned getTokenType(StringRef Value, cl::Option &O,
                        SchedGroupMask &TokenMask) {
    std::string Token = Value.str();
    std::string::const_iterator it = Token.begin();

    // Check for a complete natural number. Decimals and
    // negatives don't make sense in the context of group size,
    // and are thus not supported
    if (std::isdigit(*it)) {
      while (it != Token.end() && std::isdigit(*it))
        ++it;

      return (it == Token.end()) ? tok_number : tok_error;
    }

    if (std::isalpha(*it)) {
      // Transform the string to lower case to allow for
      // more matching
      std::transform(Token.begin(), Token.end(), Token.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      // Check if the token matches with a supported IGroup
      TokenMask = getMaskFromStr(Token);
      if (TokenMask != SchedGroupMask::NONE) {
        return handleGroup(Token, O, TokenMask);
      }
    }
    // Bad alphabetical string, or non alpha/numeric string
    return tok_error;
  }

  bool parse(cl::Option &O, StringRef ArgName, StringRef Arg,
             std::pair<SchedGroupMask, Optional<unsigned>> &Value) {
    int CurrToken = getTokenType(Arg, O, Value.first);

    if (CurrToken == tok_error)
      return O.error("Invalid Token '" + Arg + "'");

    switch (PrevToken) {
    case tok_start:
    case tok_number:
      // If there has been no token, or if the previous token was a group size,
      // then we must encounter a group name.
      if (CurrToken != tok_group)
        return O.error("Invalid Token '" + Arg + "'. Expected group token.");
      break;
    case tok_group:
      if (CurrToken == tok_number) {
        IGroupLPOrder.back().second = std::stoi(Arg.str());
        break;
      }
      // If we previously encountered a group name, and the current token is not
      // a number, then the current token must be a group name
      if (CurrToken != tok_group)
        return O.error("Invalid Token '" + Arg +
                       "'. Expected group or number token.");
      break;
    case tok_error:
    default:
      // The only other possible token value is tok_error which is already
      // handled.
      llvm_unreachable("Unsupported Token occured");
    }

    PrevToken = CurrToken;
    return 0;
  }
};

static cl::list<std::string,
                SmallVector<std::pair<SchedGroupMask, Optional<unsigned>>, 8>,
                IGroupOrderParser>
    List("amdgpu-igrouplp-order",
         cl::desc("This option is used to specify the order of groups and "
                  "their sizes to be used in AMDGPUIGroupLP. To specify, "
                  "enter a comma seperated list of groups in {salu, valu, "
                  "mfma, dsr, dsw, vmemr, vmemw, vmem} and an optional size "
                  "after each."),
         cl::CommaSeparated, cl::location(IGroupLPOrder));

// Classify instructions into groups to enable fine tuned control over the
// scheduler. These groups may be more specific than current SchedModel
// instruction classes.
class SchedGroup {
private:
  // Mask that defines which instruction types can be classified into this
  // SchedGroup. The instruction types correspond to the mask from SCHED_BARRIER
  // and SCHED_GROUP_BARRIER.
  SchedGroupMask SGMask;

  // Use SGMask to determine whether we can classify MI as a member of this
  // SchedGroup object.
  bool canAddMI(const MachineInstr &MI) const {
    bool Result = false;
    if (MI.isMetaInstruction())
      Result = false;

    else if (((SGMask & SchedGroupMask::ALU) != SchedGroupMask::NONE) &&
             (TII->isVALU(MI) || TII->isMFMA(MI) || TII->isSALU(MI)))
      Result = true;

    else if (((SGMask & SchedGroupMask::VALU) != SchedGroupMask::NONE) &&
             TII->isVALU(MI) && !TII->isMFMA(MI))
      Result = true;

    else if (((SGMask & SchedGroupMask::SALU) != SchedGroupMask::NONE) &&
             TII->isSALU(MI))
      Result = true;

    else if (((SGMask & SchedGroupMask::MFMA) != SchedGroupMask::NONE) &&
             TII->isMFMA(MI))
      Result = true;

    else if (((SGMask & SchedGroupMask::VMEM) != SchedGroupMask::NONE) &&
             (TII->isVMEM(MI) || (TII->isFLAT(MI) && !TII->isDS(MI))))
      Result = true;

    else if (((SGMask & SchedGroupMask::VMEM_READ) != SchedGroupMask::NONE) &&
             MI.mayLoad() &&
             (TII->isVMEM(MI) || (TII->isFLAT(MI) && !TII->isDS(MI))))
      Result = true;

    else if (((SGMask & SchedGroupMask::VMEM_WRITE) != SchedGroupMask::NONE) &&
             MI.mayStore() &&
             (TII->isVMEM(MI) || (TII->isFLAT(MI) && !TII->isDS(MI))))
      Result = true;

    else if (((SGMask & SchedGroupMask::DS) != SchedGroupMask::NONE) &&
             TII->isDS(MI))
      Result = true;

    else if (((SGMask & SchedGroupMask::DS_READ) != SchedGroupMask::NONE) &&
             MI.mayLoad() && TII->isDS(MI))
      Result = true;

    else if (((SGMask & SchedGroupMask::DS_WRITE) != SchedGroupMask::NONE) &&
             MI.mayStore() && TII->isDS(MI))
      Result = true;

    LLVM_DEBUG(dbgs() << "For SchedGroup with mask "
                      << format_hex((int)SGMask, 10, true)
                      << (Result ? " added " : " unable to add ") << MI);

    return Result;
  }

  // Maximum number of SUnits that can be added to this group.
  Optional<unsigned> MaxSize;

  // Collection of SUnits that are classified as members of this group.
  SmallVector<SUnit *, 32> Collection;

  ScheduleDAGInstrs *DAG = nullptr;

  const SIInstrInfo *TII;

  void tryAddEdge(SUnit *A, SUnit *B) {
    if (A != B && DAG->canAddEdge(B, A)) {
      DAG->addEdge(B, SDep(A, SDep::Artificial));
      LLVM_DEBUG(dbgs() << "Adding edge...\n"
                        << "from: SU(" << A->NodeNum << ") " << *A->getInstr()
                        << "to: SU(" << B->NodeNum << ") " << *B->getInstr());
    }
  }

public:
  // Add DAG dependencies from all SUnits in this SchedGroup and this SU. If
  // MakePred is true, SU will be a predecessor of the SUnits in this
  // SchedGroup, otherwise SU will be a successor.
  void link(SUnit &SU, bool MakePred = false) {
    for (auto A : Collection) {
      SUnit *B = &SU;
      if (MakePred)
        std::swap(A, B);

      tryAddEdge(A, B);
    }
  }

  // Add DAG dependencies from all SUnits in this SchedGroup and this SU. Use
  // the predicate to determine whether SU should be a predecessor (P = true)
  // or a successor (P = false) of this SchedGroup.
  void link(SUnit &SU, function_ref<bool(const SUnit *A, const SUnit *B)> P) {
    for (auto A : Collection) {
      SUnit *B = &SU;
      if (P(A, B))
        std::swap(A, B);

      tryAddEdge(A, B);
    }
  }

  // Add DAG dependencies such that SUnits in this group shall be ordered
  // before SUnits in OtherGroup.
  void link(SchedGroup &OtherGroup) {
    for (auto B : OtherGroup.Collection)
      link(*B);
  }

  // Returns true if no more instructions may be added to this group.
  bool isFull() { return MaxSize && Collection.size() >= *MaxSize; }

  // Returns true if SU can be added to this SchedGroup.
  bool canAddSU(SUnit &SU, const SIInstrInfo *TII) {
    if (isFull())
      return false;

    MachineInstr &MI = *SU.getInstr();
    if (MI.getOpcode() != TargetOpcode::BUNDLE) {
      return canAddMI(MI);
    }
    // Special case for bundled MIs.
    const MachineBasicBlock *MBB = MI.getParent();
    MachineBasicBlock::instr_iterator B = MI.getIterator(), E = ++B;
    while (E != MBB->end() && E->isBundledWithPred())
      ++E;

    // Return true if all of the bundled MIs can be added to this group.
    return std::all_of(B, E, [this](MachineInstr &MI) { return canAddMI(MI); });
  }

  void add(SUnit &SU) { Collection.push_back(&SU); }

  SchedGroup(SchedGroupMask SGMask) : SGMask(SGMask) {}

  SchedGroup(SchedGroupMask SGMask, Optional<unsigned> MaxSize,
             ScheduleDAGInstrs *DAG)
      : SGMask(SGMask), MaxSize(MaxSize), DAG(DAG) {}

  SchedGroup(SchedGroupMask SGMask, Optional<unsigned> MaxSize,
             ScheduleDAGInstrs *DAG, const SIInstrInfo *TII)
      : SGMask(SGMask), MaxSize(MaxSize), DAG(DAG), TII(TII) {}

};

class IGroupLPDAGMutation : public ScheduleDAGMutation {
public:
  const SIInstrInfo *TII;
  ScheduleDAGMI *DAG;

  IGroupLPDAGMutation() = default;
  void apply(ScheduleDAGInstrs *DAGInstrs) override;
};

// DAG mutation that coordinates with the SCHED_BARRIER instruction and
// corresponding builtin. The mutation adds edges from specific instruction
// classes determined by the SCHED_BARRIER mask so that they cannot be
// scheduled around the SCHED_BARRIER.
class SchedBarrierDAGMutation : public ScheduleDAGMutation {
private:
  const SIInstrInfo *TII;

  ScheduleDAGMI *DAG;

  // Cache SchedGroups of each type if we have multiple SCHED_BARRIERs in a
  // region.
  //
  std::unique_ptr<SchedGroup> MFMASchedGroup = nullptr;
  std::unique_ptr<SchedGroup> VALUSchedGroup = nullptr;
  std::unique_ptr<SchedGroup> SALUSchedGroup = nullptr;
  std::unique_ptr<SchedGroup> VMEMReadSchedGroup = nullptr;
  std::unique_ptr<SchedGroup> VMEMWriteSchedGroup = nullptr;
  std::unique_ptr<SchedGroup> DSWriteSchedGroup = nullptr;
  std::unique_ptr<SchedGroup> DSReadSchedGroup = nullptr;

  // Use a SCHED_BARRIER's mask to identify instruction SchedGroups that should
  // not be reordered accross the SCHED_BARRIER.
  void getSchedGroupsFromMask(int32_t Mask,
                              SmallVectorImpl<SchedGroup *> &SchedGroups);

  // Add DAG edges that enforce SCHED_BARRIER ordering.
  void addSchedBarrierEdges(SUnit &SU);

  // Classify instructions and add them to the SchedGroup.
  void initSchedGroup(SchedGroup *SG);

  // Remove all existing edges from a SCHED_BARRIER.
  void resetSchedBarrierEdges(SUnit &SU);

public:
  void apply(ScheduleDAGInstrs *DAGInstrs) override;

  SchedBarrierDAGMutation() = default;
};

void IGroupLPDAGMutation::apply(ScheduleDAGInstrs *DAGInstrs) {
  const GCNSubtarget &ST = DAGInstrs->MF.getSubtarget<GCNSubtarget>();
  TII = ST.getInstrInfo();
  DAG = static_cast<ScheduleDAGMI *>(DAGInstrs);
  const TargetSchedModel *TSchedModel = DAGInstrs->getSchedModel();
  if (!TSchedModel || DAG->SUnits.empty())
    return;

  LLVM_DEBUG(dbgs() << "Applying IGroupLPDAGMutation...\n");

  // The order of InstructionGroups in this vector defines the
  // order in which edges will be added. In other words, given the
  // present ordering, we will try to make each VMEMRead instruction
  // a predecessor of each DSRead instruction, and so on.
  SmallVector<SchedGroup, 8> PipelineOrderGroups;

  // Since the input string has been pre-parsed, we know we have a
  // well formed sequence of well formed strings. They will start with
  // an IGroup and will optinally be followed by a size.
  if (IGroupLPOrder.size() > 0) {
    for (auto &Stage : IGroupLPOrder) {
      PipelineOrderGroups.push_back(
          SchedGroup(Stage.first, Stage.second, DAG, TII));
    }
  }

  // Default to backwardsly compatible behavior
  else {
    PipelineOrderGroups = {
        SchedGroup(SchedGroupMask::VMEM, None, DAG, TII),
        SchedGroup(SchedGroupMask::DS_READ, None, DAG, TII),
        SchedGroup(SchedGroupMask::MFMA, None, DAG, TII),
        SchedGroup(SchedGroupMask::DS_WRITE, None, DAG, TII)};
  }

  for (SUnit &SU : DAG->SUnits) {
    LLVM_DEBUG(dbgs() << "Checking Node"; DAG->dumpNode(SU));
    for (auto &SG : PipelineOrderGroups)
      if (SG.canAddSU(SU, TII))
        SG.add(SU);
  }

  for (unsigned i = 0; i < PipelineOrderGroups.size() - 1; i++) {
    auto &GroupA = PipelineOrderGroups[i];
    for (unsigned j = i + 1; j < PipelineOrderGroups.size(); j++) {
      auto &GroupB = PipelineOrderGroups[j];
      GroupA.link(GroupB);
    }
  }
}

void SchedBarrierDAGMutation::apply(ScheduleDAGInstrs *DAGInstrs) {
  const TargetSchedModel *TSchedModel = DAGInstrs->getSchedModel();
  if (!TSchedModel || DAGInstrs->SUnits.empty())
    return;

  LLVM_DEBUG(dbgs() << "Applying SchedBarrierDAGMutation...\n");

  const GCNSubtarget &ST = DAGInstrs->MF.getSubtarget<GCNSubtarget>();
  TII = ST.getInstrInfo();
  DAG = static_cast<ScheduleDAGMI *>(DAGInstrs);
  for (auto &SU : DAG->SUnits)
    if (SU.getInstr()->getOpcode() == AMDGPU::SCHED_BARRIER)
      addSchedBarrierEdges(SU);
}

void SchedBarrierDAGMutation::addSchedBarrierEdges(SUnit &SchedBarrier) {
  MachineInstr &MI = *SchedBarrier.getInstr();
  assert(MI.getOpcode() == AMDGPU::SCHED_BARRIER);
  // Remove all existing edges from the SCHED_BARRIER that were added due to the
  // instruction having side effects.
  resetSchedBarrierEdges(SchedBarrier);
  SmallVector<SchedGroup *, 4> SchedGroups;
  int32_t Mask = MI.getOperand(0).getImm();
  getSchedGroupsFromMask(Mask, SchedGroups);
  for (auto SG : SchedGroups)
    SG->link(
        SchedBarrier, (function_ref<bool(const SUnit *A, const SUnit *B)>)[](
                          const SUnit *A, const SUnit *B) {
          return A->NodeNum > B->NodeNum;
        });
}

void SchedBarrierDAGMutation::getSchedGroupsFromMask(
    int32_t Mask, SmallVectorImpl<SchedGroup *> &SchedGroups) {
  SchedGroupMask SBMask = (SchedGroupMask)Mask;
  // See IntrinsicsAMDGPU.td for an explanation of these masks and their
  // mappings.
  //
  if ((SBMask & SchedGroupMask::VALU) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::ALU) == SchedGroupMask::NONE) {
    if (!VALUSchedGroup) {
      VALUSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::VALU, None, DAG);
      initSchedGroup(VALUSchedGroup.get());
    }

    SchedGroups.push_back(VALUSchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::SALU) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::ALU) == SchedGroupMask::NONE) {
    if (!SALUSchedGroup) {
      SALUSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::SALU, None, DAG);
      initSchedGroup(SALUSchedGroup.get());
    }

    SchedGroups.push_back(SALUSchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::MFMA) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::ALU) == SchedGroupMask::NONE) {
    if (!MFMASchedGroup) {
      MFMASchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::MFMA, None, DAG);
      initSchedGroup(MFMASchedGroup.get());
    }

    SchedGroups.push_back(MFMASchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::VMEM_READ) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::VMEM) == SchedGroupMask::NONE) {
    if (!VMEMReadSchedGroup) {
      VMEMReadSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::VMEM_READ, None, DAG);
      initSchedGroup(VMEMReadSchedGroup.get());
    }

    SchedGroups.push_back(VMEMReadSchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::VMEM_WRITE) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::VMEM) == SchedGroupMask::NONE) {
    if (!VMEMWriteSchedGroup) {
      VMEMWriteSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::VMEM_WRITE, None, DAG);
      initSchedGroup(VMEMWriteSchedGroup.get());
    }

    SchedGroups.push_back(VMEMWriteSchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::DS_READ) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::DS) == SchedGroupMask::NONE) {
    if (!DSReadSchedGroup) {
      DSReadSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::DS_READ, None, DAG);
      initSchedGroup(DSReadSchedGroup.get());
    }

    SchedGroups.push_back(DSReadSchedGroup.get());
  }

  if ((SBMask & SchedGroupMask::DS_WRITE) == SchedGroupMask::NONE &&
      (SBMask & SchedGroupMask::DS) == SchedGroupMask::NONE) {
    if (!DSWriteSchedGroup) {
      DSWriteSchedGroup =
          std::make_unique<SchedGroup>(SchedGroupMask::DS_WRITE, None, DAG);
      initSchedGroup(DSWriteSchedGroup.get());
    }

    SchedGroups.push_back(DSWriteSchedGroup.get());
  }
}

void SchedBarrierDAGMutation::initSchedGroup(SchedGroup *SG) {
  assert(SG);
  for (auto &SU : DAG->SUnits)
    if (SG->canAddSU(SU, TII))
      SG->add(SU);
}

void SchedBarrierDAGMutation::resetSchedBarrierEdges(SUnit &SU) {
  assert(SU.getInstr()->getOpcode() == AMDGPU::SCHED_BARRIER);
  for (auto &P : SU.Preds)
    SU.removePred(P);

  for (auto &S : SU.Succs) {
    for (auto &SP : S.getSUnit()->Preds) {
      if (SP.getSUnit() == &SU) {
        S.getSUnit()->removePred(SP);
      }
    }
  }
}

} // namespace

namespace llvm {

std::unique_ptr<ScheduleDAGMutation> createIGroupLPDAGMutation() {
  return EnableIGroupLP ? std::make_unique<IGroupLPDAGMutation>() : nullptr;
}

std::unique_ptr<ScheduleDAGMutation> createSchedBarrierDAGMutation() {
  return std::make_unique<SchedBarrierDAGMutation>();
}

} // end namespace llvm
