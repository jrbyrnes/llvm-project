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
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/TargetOpcodes.h"

using namespace llvm;

#define DEBUG_TYPE "igrouplp"

namespace {

static cl::opt<bool>
    EnableIGroupLP("amdgpu-igrouplp",
                   cl::desc("Enable construction of Instruction Groups and "
                            "their ordering for scheduling"),
                   cl::init(false));

static cl::opt<Optional<unsigned>>
    VMEMGroupMaxSize("amdgpu-igrouplp-vmem-group-size", cl::init(None),
                     cl::Hidden,
                     cl::desc("The maximum number of instructions to include "
                              "in VMEM group."));

static cl::opt<Optional<unsigned>>
    MFMAGroupMaxSize("amdgpu-igrouplp-mfma-group-size", cl::init(None),
                     cl::Hidden,
                     cl::desc("The maximum number of instructions to include "
                              "in MFMA group."));

static cl::opt<Optional<unsigned>>
    LDRGroupMaxSize("amdgpu-igrouplp-ldr-group-size", cl::init(None),
                    cl::Hidden,
                    cl::desc("The maximum number of instructions to include "
                             "in lds/gds read group."));

static cl::opt<Optional<unsigned>>
    LDWGroupMaxSize("amdgpu-igrouplp-ldw-group-size", cl::init(None),
                    cl::Hidden,
                    cl::desc("The maximum number of instructions to include "
                             "in lds/gds write group."));

static cl::opt<bool> EnableExactSolver(
    "amdgpu-igrouplp-exact-solver",
    cl::desc("Whether to use the exponential time solver to fit "
             "the instructions to the pipeline as closely as "
             "possible."),
    cl::init(false));

static cl::opt<bool> EnableGreedySolver(
    "amdgpu-igrouplp-greedy-solver",
    cl::desc("Whether to use the greedy solver to fit "
             "the instructions to the pipeline as closely as "
             "possible."),
    cl::init(false));

static cl::opt<int> CutoffForExact(
    "amdgpu-igrouplp-exact-solver-cutoff",
    cl::desc("The maximum number of scheduling group conflicts "
             "which we attempt to solve with the exponential time "
             "exact solver. Problem sizes greater than this will"
             "be solved by the less accurate greedy algorithm. Selecting "
             "solver by size is superseded by manually selecting "
             "the solver (e.g. by amdgpu-igrouplp-exact-solver"),
    cl::init(30));

// Components of the mask that determines which instruction types may be may be
// classified into a SchedGroup.
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

struct SchedGroupSU {
  SUnit *SU;
  SmallVector<SmallVector<int, 8>, 4> Matches;

  SchedGroupSU(SUnit *SU) : SU(SU) {}
};

// Classify instructions into groups to enable fine tuned control over the
// scheduler. These groups may be more specific than current SchedModel
// instruction classes.
class SchedGroup {
private:
  // Mask that defines which instruction types can be classified into this
  // SchedGroup. The instruction types correspond to the mask from SCHED_BARRIER
  // and SCHED_GROUP_BARRIER.
  SchedGroupMask SGMask;

  // Maximum number of SUnits that can be added to this group.
  Optional<unsigned> MaxSize;

  // SchedGroups will only synchronize with other SchedGroups that have the same
  // SyncID.
  int SyncID = 0;

  // Collection of SUnits that are classified as members of this group.
  //  SmallVector<SUnit *, 32> Collection;

  ScheduleDAGInstrs *DAG;

  const SIInstrInfo *TII;

  // Try to add and edge from SU A to SU B.
  bool tryAddEdge(SUnit *A, SUnit *B);

  // Use SGMask to determine whether we can classify MI as a member of this
  // SchedGroup object.
  bool canAddMI(const MachineInstr &MI) const;

public:
  // Collection of SUnits that are classified as members of this group.
  SmallVector<SUnit *, 32> Collection;

  std::vector<SUnit>::reverse_iterator BarrierPosition;
  // Returns true if SU can be added to this SchedGroup.
  bool canAddSU(SUnit &SU) const;

  // Add DAG dependencies from all SUnits in this SchedGroup and this SU. If
  // MakePred is true, SU will be a predecessor of the SUnits in this
  // SchedGroup, otherwise SU will be a successor.
  void link(SUnit &SU, bool MakePred = false);

  // Add DAG dependencies and track which edges are added, and the count of
  // missed edges
  int link(SUnit &SU, bool MakePred,
           std::vector<std::pair<SUnit *, SUnit *>> &AddedEdges);

  // Add DAG dependencies from all SUnits in this SchedGroup and this SU.
  // Use the predicate to determine whether SU should be a predecessor (P =
  // true) or a successor (P = false) of this SchedGroup.
  void link(SUnit &SU, function_ref<bool(const SUnit *A, const SUnit *B)> P);

  // Add DAG dependencies such that SUnits in this group shall be ordered
  // before SUnits in OtherGroup.
  void link(SchedGroup &OtherGroup);

  // Returns true if no more instructions may be added to this group.
  bool isFull() const { return MaxSize && Collection.size() >= *MaxSize; }

  // Add SU to the SchedGroup.
  void add(SUnit &SU) {
    LLVM_DEBUG(dbgs() << "For SchedGroup with mask "
                      << format_hex((int)SGMask, 10, true) << " adding "
                      << *SU.getInstr());
    Collection.push_back(&SU);
  }

  // Remove last element in the SchedGroup
  void pop() { Collection.pop_back(); }

  // Identify and add all relevant SUs from the DAG to this SchedGroup.
  void initSchedGroup();

  // Add instructions to the SchedGroup bottom up starting from RIter.
  // ConflictedInstrs is a set of instructions that should not be added to the
  // SchedGroup even when the other conditions for adding it are satisfied.
  // RIter will be added to the SchedGroup as well, and dependencies will be
  // added so that RIter will always be scheduled at the end of the group.
  void initSchedGroup(std::vector<SUnit>::reverse_iterator RIter,
                      DenseSet<SUnit *> &ConflictedInstrs);

  int getSyncID() { return SyncID; }

  SchedGroupMask getMask() { return SGMask; }

  SchedGroup() {}

  SchedGroup(SchedGroupMask SGMask, Optional<unsigned> MaxSize,
             ScheduleDAGInstrs *DAG, const SIInstrInfo *TII)
      : SGMask(SGMask), MaxSize(MaxSize), DAG(DAG), TII(TII) {}

  SchedGroup(SchedGroupMask SGMask, Optional<unsigned> MaxSize, int SyncID,
             ScheduleDAGInstrs *DAG, const SIInstrInfo *TII,
             std::vector<SUnit>::reverse_iterator BarrierPosition)
      : SGMask(SGMask), MaxSize(MaxSize), SyncID(SyncID), DAG(DAG), TII(TII),
        BarrierPosition(BarrierPosition) {}
};

// The PipelineSolver is used to assign SUnits to SchedGroups in a pipeline
// in non-trivial cases. For example, if the requested pipeline is
// {VMEM_READ, VALU, MFMA, VMEM_READ} and we encounter a VMEM_READ instruction
// in the DAG, then we will have an instruction that can not be trivially
// assigned to a SchedGroup. The PipelineSolver class implements two algorithms
// to find a good solution to the pipeline -- a greedy algorithm and an exact
// algorithm. The exact algorithm has an exponential time complexity and should
// only be used for small sized problems or medium sized problems where an exact
// solution is highly desired.

class PipelineSolver {
  // Instructions that can be assigned to multiple SchedGroups
  const SmallVector<SmallVector<SchedGroupSU, 16>, 4> ConflictedInstrs;
  ScheduleDAGMI *DAG;

  // The current working pipeline
  SmallVector<SmallVector<SchedGroup, 4>, 4> CurrPipeline;
  // The pipeline that has the best solution found so far
  SmallVector<SmallVector<SchedGroup, 4>, 4> BestPipeline;

  // Compute an estimate of the size of search tree -- the true size is
  // the product of each conflictedInst.Matches.size() across all SyncPipelines
  int computeProblemSize();

  // The cost penalty of not assigning a SU to a SchedGroup
  int MissPenalty = 0;

  // Costs in terms of the number of edges we are unable to add
  int BestCost = -1;
  int CurrCost = 0;

  // Index pointing to the conflicting instruction that is currently being
  // fitted
  int CurrConflInstNo = 0;
  // Index to the pipeline that is currently being fitted
  int CurrSyncGroupIdx = 0;
  // The first non trivial pipeline
  int BeginSyncGroupIdx = 0;

  // Update indices to fit next conflicting instruction
  void advancePosition();
  // Recede indices to attempt to find better fit for previous conflicting
  // instruction
  void retreatPosition();

  // The exponential time algorithm which finds the provably best fit
  bool solveExact();
  // The polynomial time algorithm which attempts to find a good fit
  bool solveGreedy();
  // Whether or not the current solution is optimal
  bool checkOptimal();
  // Add edges corresponding to the SchedGroups as assigned by solver
  void makePipeline();
  // Add the edges from the SU to the other SchedGroups in pipeline, and
  // return the number of edges missed.
  int addEdges(SmallVector<SchedGroup, 4> &SyncPipeline, SUnit *SU,
               int AssignedGroupNo,
               std::vector<std::pair<SUnit *, SUnit *>> &AddedEdges);
  // Remove the edges passed via AddedEdges
  void removeEdges(const std::vector<std::pair<SUnit *, SUnit *>> &AddedEdges);

public:
  // Invoke the solver to map instructions to instruction groups. Heuristic &&
  // command-line-option determines to use exact or greedy algorithm.
  void solve();

  PipelineSolver(
      SmallVector<SmallVector<SchedGroup, 4>, 4> &Pipeline,
      const SmallVector<SmallVector<SchedGroupSU, 16>, 4> &ConflictedInstrs,
      ScheduleDAGMI *DAG)
      : ConflictedInstrs(ConflictedInstrs), DAG(DAG), BestPipeline(Pipeline) {
    CurrPipeline = BestPipeline;
    while (static_cast<size_t>(BeginSyncGroupIdx) < ConflictedInstrs.size() &&
           ConflictedInstrs[BeginSyncGroupIdx].size() == 0)
      ++BeginSyncGroupIdx;

    if (static_cast<size_t>(BeginSyncGroupIdx) >= ConflictedInstrs.size())
      return;

    CurrSyncGroupIdx = BeginSyncGroupIdx;
  }
};

void PipelineSolver::makePipeline() {
  // Preserve the order of barrier for subsequent SchedGroupBarrier mutations
  for (auto &SyncPipeline : BestPipeline) {
    for (auto &SG : SyncPipeline) {
      SG.link(*SG.BarrierPosition,
              (function_ref<bool(const SUnit *A, const SUnit *B)>)[](
                  const SUnit *A, const SUnit *B) {
                return A->NodeNum > B->NodeNum;
              });
    }
  }

  for (auto &SyncPipeline : BestPipeline) {
    auto I = SyncPipeline.rbegin();
    auto E = SyncPipeline.rend();
    for (; I != E; ++I) {
      auto &GroupA = *I;
      for (auto J = std::next(I); J != E; ++J) {
        auto &GroupB = *J;
        GroupA.link(GroupB);
      }
    }
  }
}

int PipelineSolver::addEdges(
    SmallVector<SchedGroup, 4> &SyncPipeline, SUnit *SU,
    const int AssignedGroupNo,
    std::vector<std::pair<SUnit *, SUnit *>> &AddedEdges) {
  int AddedCost = 0;
  bool MakePred = false;

  // The groups in the pipeline are in reverse order. Thus,
  // by traversing them from last to first, we are traversing
  // them in the order as they were introduced in the code. After we
  // pass the group the SU is being assigned to, it should be
  // linked as a predecessor of the subsequent SchedGroups
  auto GroupNo = (int)SyncPipeline.size() - 1;
  for (; GroupNo >= 0; GroupNo--) {
    if (AssignedGroupNo == GroupNo) {
      MakePred = true;
      continue;
    }
    assert(MakePred || GroupNo > AssignedGroupNo);
    auto Group = &SyncPipeline[GroupNo];
    AddedCost += Group->link(*SU, MakePred, AddedEdges);
    assert(AddedCost >= 0);
  }

  return AddedCost;
}

void PipelineSolver::removeEdges(
    const std::vector<std::pair<SUnit *, SUnit *>> &EdgesToRemove) {
  // Only remove the edges that we have added when testing
  // the fit.
  for (auto &PredSuccPair : EdgesToRemove) {
    SUnit *Pred = PredSuccPair.first;
    SUnit *Succ = PredSuccPair.second;

    auto Match =
        std::find_if(Succ->Preds.begin(), Succ->Preds.end(),
                     [&Pred](SDep &P) { return P.getSUnit() == Pred; });
    if (Match != Succ->Preds.end()) {
      Succ->removePred(*Match);
    }
  }
}

void PipelineSolver::advancePosition() {
  ++CurrConflInstNo;

  if (static_cast<size_t>(CurrConflInstNo) >=
      ConflictedInstrs[CurrSyncGroupIdx].size()) {
    CurrConflInstNo = 0;
    ++CurrSyncGroupIdx;
    // Advance to next non-trivial pipeline
    while (static_cast<size_t>(CurrSyncGroupIdx) < ConflictedInstrs.size() &&
           ConflictedInstrs[CurrSyncGroupIdx].size() == 0)
      ++CurrSyncGroupIdx;
  }
}

void PipelineSolver::retreatPosition() {
  assert(CurrConflInstNo >= 0);
  assert(CurrSyncGroupIdx >= 0);
  if (CurrConflInstNo > 0) {
    --CurrConflInstNo;

    return;
  }

  if (CurrConflInstNo == 0) {
    // If we return to the starting position, we have explored
    // the entire tree
    if (CurrSyncGroupIdx == BeginSyncGroupIdx)
      return;

    --CurrSyncGroupIdx;
    // Go to previous non-trivial pipeline
    while (ConflictedInstrs[CurrSyncGroupIdx].size() == 0)
      --CurrSyncGroupIdx;

    CurrConflInstNo = ConflictedInstrs[CurrSyncGroupIdx].size() - 1;
  }
}

bool PipelineSolver::checkOptimal() {
  if (static_cast<size_t>(CurrSyncGroupIdx) == ConflictedInstrs.size()) {
    if (BestCost == -1 || CurrCost < BestCost) {
      BestPipeline = CurrPipeline;
      BestCost = CurrCost;
      LLVM_DEBUG(dbgs() << "Found Fit with cost " << BestCost << "\n");
    }
    assert(BestCost >= 0);
  }
  return BestCost == 0;
}

bool PipelineSolver::solveExact() {
  if (checkOptimal())
    return true;

  if (static_cast<size_t>(CurrSyncGroupIdx) == ConflictedInstrs.size())
    return false;

  assert(static_cast<size_t>(CurrSyncGroupIdx) < ConflictedInstrs.size());
  assert(static_cast<size_t>(CurrConflInstNo) <
         ConflictedInstrs[CurrSyncGroupIdx].size());
  SchedGroupSU CurrSGSU = ConflictedInstrs[CurrSyncGroupIdx][CurrConflInstNo];
  LLVM_DEBUG(dbgs() << "Fitting SU(" << CurrSGSU.SU->NodeNum
                    << ") in Pipeline # " << CurrSyncGroupIdx << "\n");
  // Since we have added the potential SchedGroups from bottom up, but
  // traversed the DAG from top down, parse over the groups from last to first.
  // In this way, the position of the instruction in the initial code more
  // closely aligns with the position of the SchedGroupBarrier relative to the
  // entire pipeline. Parsing in such a way increases likelihood of good
  // solution found early.
  auto I = CurrSGSU.Matches[CurrSyncGroupIdx].rbegin();
  auto E = CurrSGSU.Matches[CurrSyncGroupIdx].rend();
  assert(CurrSGSU.Matches.size() >= 1);
  for (; I != E; ++I) {
    int SchedGroupNo = *I;
    int AddedCost = 0;
    std::vector<std::pair<SUnit *, SUnit *>> AddedEdges;
    auto &SyncPipeline = CurrPipeline[CurrSyncGroupIdx];
    SchedGroup *Match = &SyncPipeline[SchedGroupNo];

    if (Match->isFull())
      continue;

    LLVM_DEBUG(dbgs() << "Assigning to SchedGroup with Mask "
                      << (int)Match->getMask() << "and Group # " << SchedGroupNo
                      << "\n");
    Match->add(*CurrSGSU.SU);
    AddedCost = addEdges(SyncPipeline, CurrSGSU.SU, SchedGroupNo, AddedEdges);
    LLVM_DEBUG(dbgs() << "Cost of Assignment: " << AddedCost << "\n");
    CurrCost += AddedCost;
    advancePosition();

    // If the Cost after adding edges is greater than a known solution,
    // backtrack
    if (CurrCost < BestCost || BestCost == -1) {
      if (solveExact())
        return true;
    }

    retreatPosition();
    CurrCost -= AddedCost;
    removeEdges(AddedEdges);
    Match->pop();
    CurrPipeline[CurrSyncGroupIdx] = SyncPipeline;
  }

  // Try the pipeline where the current instruction is omitted
  // Potentially if we omit a problematic instruction from the pipeline,
  // all the other instructions can nicely fit.
  CurrCost += MissPenalty;
  advancePosition();

  LLVM_DEBUG(dbgs() << "NOT Assigned (" << CurrSGSU.SU->NodeNum << ")\n");

  if (CurrCost < BestCost || BestCost == -1) {
    if (solveExact())
      return true;
  }

  retreatPosition();
  CurrCost -= MissPenalty;

  return false;
}

bool PipelineSolver::solveGreedy() {
  while (static_cast<size_t>(CurrSyncGroupIdx) < ConflictedInstrs.size()) {
    SchedGroupSU CurrSGSU = ConflictedInstrs[CurrSyncGroupIdx][CurrConflInstNo];
    int BestCost = -1;
    int TempCost;
    SchedGroup *BestGroup = nullptr;
    int BestGroupNo = -1;
    auto &SyncPipeline = BestPipeline[CurrSyncGroupIdx];
    LLVM_DEBUG(dbgs() << "Fitting SU(" << CurrSGSU.SU->NodeNum
                      << ") in Pipeline # " << CurrSyncGroupIdx << "\n");

    // Since we have added the potential SchedGroups from bottom up, but
    // traversed the DAG from top down, parse over the groups from last to
    // first. If we fail to do this for the greedy algorithm, the solution will
    // likely not be good in more complex cases.
    auto I = CurrSGSU.Matches[CurrSyncGroupIdx].rbegin();
    auto E = CurrSGSU.Matches[CurrSyncGroupIdx].rend();
    for (; I != E; ++I) {
      int SchedGroupNo = *I;
      std::vector<std::pair<SUnit *, SUnit *>> AddedEdges;
      SchedGroup *Match = &SyncPipeline[SchedGroupNo];
      LLVM_DEBUG(dbgs() << "Trying Group # " << SchedGroupNo << " with Mask "
                        << (int)Match->getMask() << "\n");
      if (Match->isFull()) {
        LLVM_DEBUG(dbgs() << "Group # " << SchedGroupNo << " is full\n");
        continue;
      }
      TempCost = addEdges(SyncPipeline, CurrSGSU.SU, SchedGroupNo, AddedEdges);
      LLVM_DEBUG(dbgs() << "Cost of Group " << TempCost << "\n");
      if (TempCost < BestCost || BestCost == -1) {
        BestGroup = Match;
        BestCost = TempCost;
        BestGroupNo = SchedGroupNo;
      }
      removeEdges(AddedEdges);
      if (BestCost == 0)
        break;
    }

    if (BestGroup) {
      BestGroup->add(*CurrSGSU.SU);
      std::vector<std::pair<SUnit *, SUnit *>> AddedEdges;
      addEdges(SyncPipeline, CurrSGSU.SU, BestGroupNo, AddedEdges);
      LLVM_DEBUG(dbgs() << "Best Group has GroupNo: " << BestGroupNo
                        << " and Mask" << (int)BestGroup->getMask() << "\n");
    }
    advancePosition();
  }

  return false;
}

int PipelineSolver::computeProblemSize() {
  int ProblemSize = 0;
  for (auto &PipeConflicts : ConflictedInstrs) {
    ProblemSize += PipeConflicts.size();
  }

  return ProblemSize;
}

void PipelineSolver::solve() {
  bool ShouldUseExact;
  int ProblemSize = computeProblemSize();
  assert(ProblemSize > 0);
  bool SmallProblem = ProblemSize <= CutoffForExact;

  ShouldUseExact = EnableExactSolver || (!EnableGreedySolver && SmallProblem);

  MissPenalty = (ProblemSize / 2) + 1;
  assert(MissPenalty > 0);

  LLVM_DEBUG(DAG->dump());
  if (ShouldUseExact) {
    LLVM_DEBUG(dbgs() << "Starting EXACT pipeline solver\n");
    solveExact();
  } else {
    LLVM_DEBUG(dbgs() << "Starting GREEDY pipeline solver\n");
    solveGreedy();
  }

  makePipeline();
}

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
class SchedBarrierDAGMutation : public ScheduleDAGMutation {
private:
  const SIInstrInfo *TII;

  ScheduleDAGMI *DAG;

  // Convert a user inputted SchedGroupBarrier ID to an index
  // in an array holding the synchronized SchedGroups
  DenseMap<int, int> BarrierIDToPipelineID;

  // Organize lists of SchedGroups by their SyncID. SchedGroups /
  // SCHED_GROUP_BARRIERs with different SyncIDs will have no edges added
  // between then.
  SmallVector<SmallVector<SchedGroup, 4>, 4> SyncedSchedGroups;

  // Used to track instructions that can be mapped to multiple sched groups
  SmallVector<SmallVector<SchedGroupSU, 16>, 4> ConflictedInstrs;

  // Add DAG edges that enforce SCHED_BARRIER ordering.
  void addSchedBarrierEdges(SUnit &SU);

  // Use a SCHED_BARRIER's mask to identify instruction SchedGroups that should
  // not be reordered accross the SCHED_BARRIER. This is used for the base
  // SCHED_BARRIER, and not SCHED_GROUP_BARRIER. The difference is that
  // SCHED_BARRIER will always block all instructions that can be classified
  // into a particular SchedClass, whereas SCHED_GROUP_BARRIER has a fixed size
  // and may only synchronize with some SchedGroups. Returns the inverse of
  // Mask. SCHED_BARRIER's mask describes which instruction types should be
  // allowed to be scheduled across it. Invert the mask to get the
  // SchedGroupMask of instructions that should be barred.
  SchedGroupMask invertSchedBarrierMask(SchedGroupMask Mask) const;

  // Create SchedGroups for a SCHED_GROUP_BARRIER.
  void initSchedGroupBarrierPipelineStage(
      std::vector<SUnit>::reverse_iterator RIter);

  // Map the SUnits to candidate Sched Groups
  void collectPipelineSGSU();

public:
  void apply(ScheduleDAGInstrs *DAGInstrs) override;

  SchedBarrierDAGMutation() = default;
};

bool SchedGroup::tryAddEdge(SUnit *A, SUnit *B) {
  if (A != B && DAG->canAddEdge(B, A)) {
    DAG->addEdge(B, SDep(A, SDep::Artificial));
    LLVM_DEBUG(dbgs() << "Adding edge...\n"
                      << "from: SU(" << A->NodeNum << ") " << *A->getInstr()
                      << "to: SU(" << B->NodeNum << ") " << *B->getInstr());
    return true;
  }
  return false;
}

bool SchedGroup::canAddMI(const MachineInstr &MI) const {
  bool Result = false;
  if (MI.isMetaInstruction() || MI.getOpcode() == AMDGPU::SCHED_BARRIER ||
      MI.getOpcode() == AMDGPU::SCHED_GROUP_BARRIER)
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

  LLVM_DEBUG(
      dbgs() << "For SchedGroup with mask " << format_hex((int)SGMask, 10, true)
             << (Result ? " could classify " : " unable to classify ") << MI);

  return Result;
}

int SchedGroup::link(SUnit &SU, bool MakePred,
                     std::vector<std::pair<SUnit *, SUnit *>> &AddedEdges) {
  int MissedEdges = 0;
  for (auto A : Collection) {
    SUnit *B = &SU;
    if (MakePred)
      std::swap(A, B);

    bool ShouldTryAddEdge = A != B;

    // If we don't add edge because B is already recursive successor of A,
    // this is not a deviation from desired pipeline and we should not
    // increase the cost
    if (ShouldTryAddEdge && DAG->IsReachable(B, A))
      ShouldTryAddEdge = false;

    if (!ShouldTryAddEdge)
      continue;
    bool Added = tryAddEdge(A, B);
    if (Added) {
      AddedEdges.push_back(std::make_pair(A, B));
    } else
      ++MissedEdges;
  }

  return MissedEdges;
}

void SchedGroup::link(SUnit &SU, bool MakePred) {
  for (auto A : Collection) {
    SUnit *B = &SU;
    if (MakePred)
      std::swap(A, B);

    tryAddEdge(A, B);
  }
}

void SchedGroup::link(SUnit &SU,
                      function_ref<bool(const SUnit *A, const SUnit *B)> P) {
  for (auto A : Collection) {
    SUnit *B = &SU;
    if (P(A, B))
      std::swap(A, B);

    tryAddEdge(A, B);
  }
}

void SchedGroup::link(SchedGroup &OtherGroup) {
  for (auto B : OtherGroup.Collection)
    link(*B);
}

bool SchedGroup::canAddSU(SUnit &SU) const {
  MachineInstr &MI = *SU.getInstr();
  if (MI.getOpcode() != TargetOpcode::BUNDLE)
    return canAddMI(MI);

  // Special case for bundled MIs.
  const MachineBasicBlock *MBB = MI.getParent();
  MachineBasicBlock::instr_iterator B = MI.getIterator(), E = ++B;
  while (E != MBB->end() && E->isBundledWithPred())
    ++E;

  // Return true if all of the bundled MIs can be added to this group.
  return std::all_of(B, E, [this](MachineInstr &MI) { return canAddMI(MI); });
}

void SchedGroup::initSchedGroup() {
  for (auto &SU : DAG->SUnits) {
    if (isFull())
      break;

    if (canAddSU(SU))
      add(SU);
  }
}

static bool canFitIntoPipeline(SUnit &SU, ScheduleDAGInstrs *DAG,
                               DenseSet<SUnit *> &ConflictedInstrs) {
  return std::all_of(
      ConflictedInstrs.begin(), ConflictedInstrs.end(),
      [DAG, &SU](SUnit *SuccSU) { return DAG->canAddEdge(SuccSU, &SU); });
}

void SchedGroup::initSchedGroup(std::vector<SUnit>::reverse_iterator RIter,
                                DenseSet<SUnit *> &ConflictedInstrs) {
  SUnit &InitSU = *RIter;
  for (auto E = DAG->SUnits.rend(); RIter != E; ++RIter) {
    auto &SU = *RIter;
    if (isFull())
      break;

    if (canAddSU(SU) && !ConflictedInstrs.count(&SU) &&
        canFitIntoPipeline(SU, DAG, ConflictedInstrs)) {
      add(SU);
      ConflictedInstrs.insert(&SU);
    }
  }

  add(InitSU);
  assert(MaxSize);
  (*MaxSize)++;
}

// Create a pipeline from the SchedGroups in PipelineOrderGroups such that we
// try to enforce the relative ordering of instructions in each group.
static void makePipeline(SmallVectorImpl<SchedGroup> &PipelineOrderGroups) {
  auto I = PipelineOrderGroups.begin();
  auto E = PipelineOrderGroups.end();
  for (; I != E; ++I) {
    auto &GroupA = *I;
    for (auto J = std::next(I); J != E; ++J) {
      auto &GroupB = *J;
      GroupA.link(GroupB);
    }
  }
}

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
  SmallVector<SchedGroup, 4> PipelineOrderGroups = {
      SchedGroup(SchedGroupMask::VMEM, VMEMGroupMaxSize, DAG, TII),
      SchedGroup(SchedGroupMask::DS_READ, LDRGroupMaxSize, DAG, TII),
      SchedGroup(SchedGroupMask::MFMA, MFMAGroupMaxSize, DAG, TII),
      SchedGroup(SchedGroupMask::DS_WRITE, LDWGroupMaxSize, DAG, TII)};

  for (auto &SG : PipelineOrderGroups)
    SG.initSchedGroup();

  makePipeline(PipelineOrderGroups);
}

// Remove all existing edges from a SCHED_BARRIER or SCHED_GROUP_BARRIER.
static void resetEdges(SUnit &SU, ScheduleDAGInstrs *DAG) {
  assert(SU.getInstr()->getOpcode() == AMDGPU::SCHED_BARRIER ||
         SU.getInstr()->getOpcode() == AMDGPU::SCHED_GROUP_BARRIER);

  while (!SU.Preds.empty())
    for (auto &P : SU.Preds)
      SU.removePred(P);

  while (!SU.Succs.empty())
    for (auto &S : SU.Succs)
      for (auto &SP : S.getSUnit()->Preds)
        if (SP.getSUnit() == &SU)
          S.getSUnit()->removePred(SP);
}

void SchedBarrierDAGMutation::apply(ScheduleDAGInstrs *DAGInstrs) {
  const TargetSchedModel *TSchedModel = DAGInstrs->getSchedModel();
  if (!TSchedModel || DAGInstrs->SUnits.empty())
    return;

  LLVM_DEBUG(dbgs() << "Applying SchedBarrierDAGMutation...\n");
  const GCNSubtarget &ST = DAGInstrs->MF.getSubtarget<GCNSubtarget>();
  TII = ST.getInstrInfo();
  DAG = static_cast<ScheduleDAGMI *>(DAGInstrs);

  BarrierIDToPipelineID.clear();
  SyncedSchedGroups.clear();
  ConflictedInstrs.clear();
  for (auto R = DAG->SUnits.rbegin(), E = DAG->SUnits.rend(); R != E; ++R) {
    if (R->getInstr()->getOpcode() == AMDGPU::SCHED_BARRIER)
      addSchedBarrierEdges(*R);

    else if (R->getInstr()->getOpcode() == AMDGPU::SCHED_GROUP_BARRIER)
      initSchedGroupBarrierPipelineStage(R);
  }

  collectPipelineSGSU();
  if (ConflictedInstrs.size() >= 1) {
    PipelineSolver PS(SyncedSchedGroups, ConflictedInstrs, DAG);
    PS.solve();
  }
}

void SchedBarrierDAGMutation::addSchedBarrierEdges(SUnit &SchedBarrier) {
  MachineInstr &MI = *SchedBarrier.getInstr();
  assert(MI.getOpcode() == AMDGPU::SCHED_BARRIER);
  // Remove all existing edges from the SCHED_BARRIER that were added due to the
  // instruction having side effects.
  resetEdges(SchedBarrier, DAG);
  auto InvertedMask =
      invertSchedBarrierMask((SchedGroupMask)MI.getOperand(0).getImm());
  SchedGroup SG(InvertedMask, None, DAG, TII);
  SG.initSchedGroup();
  // Preserve original instruction ordering relative to the SCHED_BARRIER.
  SG.link(
      SchedBarrier,
      (function_ref<bool(const SUnit *A, const SUnit *B)>)[](
          const SUnit *A, const SUnit *B) { return A->NodeNum > B->NodeNum; });
}

SchedGroupMask
SchedBarrierDAGMutation::invertSchedBarrierMask(SchedGroupMask Mask) const {
  // Invert mask and erase bits for types of instructions that are implied to be
  // allowed past the SCHED_BARRIER.
  SchedGroupMask InvertedMask = ~Mask;

  // ALU implies VALU, SALU, MFMA.
  if ((InvertedMask & SchedGroupMask::ALU) == SchedGroupMask::NONE)
    InvertedMask &=
        ~SchedGroupMask::VALU & ~SchedGroupMask::SALU & ~SchedGroupMask::MFMA;
  // VALU, SALU, MFMA implies ALU.
  else if ((InvertedMask & SchedGroupMask::VALU) == SchedGroupMask::NONE ||
           (InvertedMask & SchedGroupMask::SALU) == SchedGroupMask::NONE ||
           (InvertedMask & SchedGroupMask::MFMA) == SchedGroupMask::NONE)
    InvertedMask &= ~SchedGroupMask::ALU;

  // VMEM implies VMEM_READ, VMEM_WRITE.
  if ((InvertedMask & SchedGroupMask::VMEM) == SchedGroupMask::NONE)
    InvertedMask &= ~SchedGroupMask::VMEM_READ & ~SchedGroupMask::VMEM_WRITE;
  // VMEM_READ, VMEM_WRITE implies VMEM.
  else if ((InvertedMask & SchedGroupMask::VMEM_READ) == SchedGroupMask::NONE ||
           (InvertedMask & SchedGroupMask::VMEM_WRITE) == SchedGroupMask::NONE)
    InvertedMask &= ~SchedGroupMask::VMEM;

  // DS implies DS_READ, DS_WRITE.
  if ((InvertedMask & SchedGroupMask::DS) == SchedGroupMask::NONE)
    InvertedMask &= ~SchedGroupMask::DS_READ & ~SchedGroupMask::DS_WRITE;
  // DS_READ, DS_WRITE implies DS.
  else if ((InvertedMask & SchedGroupMask::DS_READ) == SchedGroupMask::NONE ||
           (InvertedMask & SchedGroupMask::DS_WRITE) == SchedGroupMask::NONE)
    InvertedMask &= ~SchedGroupMask::DS;

  return InvertedMask;
}

void SchedBarrierDAGMutation::initSchedGroupBarrierPipelineStage(
    std::vector<SUnit>::reverse_iterator RIter) {
  // Remove all existing edges from the SCHED_GROUP_BARRIER that were added due
  // to the instruction having side effects.
  resetEdges(*RIter, DAG);
  MachineInstr &SGB = *RIter->getInstr();
  assert(SGB.getOpcode() == AMDGPU::SCHED_GROUP_BARRIER);
  int32_t SGMask = SGB.getOperand(0).getImm();
  int32_t Size = SGB.getOperand(1).getImm();
  int32_t BarrierID = SGB.getOperand(2).getImm();

  int MappedIndex;
  // Convert a user inputted Pipeline / Barrier ID to an index in the array
  // which holds all the pipelines
  if (BarrierIDToPipelineID.find(BarrierID) == BarrierIDToPipelineID.end()) {
    MappedIndex = SyncedSchedGroups.size();
    BarrierIDToPipelineID[BarrierID] = MappedIndex;
    SyncedSchedGroups.resize(MappedIndex + 1);
  } else {
    MappedIndex = BarrierIDToPipelineID[BarrierID];
    assert(static_cast<size_t>(MappedIndex) < SyncedSchedGroups.size());
  }

  // Create a new SchedGroup and add it to a list that is mapped to the SyncID.
  // SchedGroups only enforce ordering between SchedGroups with the same SyncID.
  SyncedSchedGroups[MappedIndex].emplace_back((SchedGroupMask)SGMask, Size,
                                              BarrierID, DAG, TII, RIter);
}

void SchedBarrierDAGMutation::collectPipelineSGSU() {
  for (auto &SU : DAG->SUnits) {
    SchedGroupSU SGSU(&SU);
    for (int PipelineIdx = 0;
         static_cast<size_t>(PipelineIdx) < SyncedSchedGroups.size();
         PipelineIdx++) {
      for (int StageIdx = 0; static_cast<size_t>(StageIdx) <
                             SyncedSchedGroups[PipelineIdx].size();
           StageIdx++) {
        SchedGroup PipelineGroup = SyncedSchedGroups[PipelineIdx][StageIdx];
        std::vector<SUnit>::reverse_iterator RIter =
            PipelineGroup.BarrierPosition;
        if (!PipelineGroup.canAddSU(SU))
          continue;

        auto TempIter = RIter;

        auto Match =
            std::find_if(TempIter, DAG->SUnits.rend(),
                         [&SU](SUnit &IterSU) { return &SU == &IterSU; });

        if (Match != DAG->SUnits.rend()) {
          // Grow the SGSU matches to hold the new match
          if (static_cast<size_t>(PipelineIdx) >= SGSU.Matches.size())
            SGSU.Matches.resize(PipelineIdx + 1);
          SGSU.Matches[PipelineIdx].push_back(StageIdx);
        }
      }
      if (static_cast<size_t>(PipelineIdx) >= SGSU.Matches.size())
        continue; // The SGSU is not included in current Sync Pipeline

      if (SGSU.Matches[PipelineIdx].size() >= 1) {
        // Grow the ConflictedInstrs to hold the pipeline instructions
        if (static_cast<size_t>(PipelineIdx) >= ConflictedInstrs.size())
          ConflictedInstrs.resize(PipelineIdx + 1);
        ConflictedInstrs[PipelineIdx].push_back(SGSU);
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
