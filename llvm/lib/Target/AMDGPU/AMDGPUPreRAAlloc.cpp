//===- AMDGPUPreRAAlloc.cpp - Pre-RA Hint Generator for VGPR Allocation ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tier-1 MSB stall reduction for gfx1250+. Clusters DS_READ dest/addr/NextMI
// operands, merges via shared vregs, and sets hints for Greedy allocator.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "GCNSubtarget.h"
#include "SIInstrInfo.h"
#include "SIRegisterInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallBitVector.h"
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

#define DEBUG_TYPE "amdgpu-pre-ra-alloc"

static cl::opt<bool> EnablePreRAAlloc(
    "amdgpu-pre-ra-alloc",
    cl::desc("Enable AMDGPU Pre-RA mini allocator for VGPR hints"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> SeedOnlyFromLoops(
    "amdgpu-pre-ra-alloc-loops-only",
    cl::desc("Only seed DS_READs from within loops (focus on hot code)"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> AnchorOnlyMode(
    "amdgpu-pre-ra-anchor-only",
    cl::desc("Only allocate when there's a physical anchor (conservative mode)"),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> DefaultBlock(
    "amdgpu-pre-ra-default-block",
    cl::desc("Default block when no anchor (0-3, 4=least-pressured)"),
    cl::init(4), cl::Hidden);

static cl::opt<bool> EnablePostAnalysis(
    "amdgpu-pre-ra-alloc-analysis",
    cl::desc("Print analysis of what was allocated vs missed"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> WMMAWindowOnlyMode(
    "amdgpu-pre-ra-wmma-window-only",
    cl::desc("Only hint clusters within WMMA co-execution windows (reduces spill pressure)"),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> BatchSizeTarget(
    "amdgpu-pre-ra-batch-size",
    cl::desc("Target batch size in lanes for greedy block packing"),
    cl::init(200), cl::Hidden);

static cl::opt<unsigned> BatchMaxClusters(
    "amdgpu-pre-ra-batch-max-clusters",
    cl::desc("Maximum clusters per batch to limit retry loops"),
    cl::init(6), cl::Hidden);

static cl::opt<unsigned> LargeTupleThreshold(
    "amdgpu-pre-ra-large-tuple",
    cl::desc("Tuple size (in lanes) above which allocation failure triggers retry"),
    cl::init(8), cl::Hidden);

static cl::opt<unsigned> HighFanoutThreshold(
    "amdgpu-pre-ra-fanout-threshold",
    cl::desc("VReg use count threshold for high fanout"),
    cl::init(4), cl::Hidden);

static cl::opt<unsigned> SpatialWindowSize(
    "amdgpu-pre-ra-spatial-window",
    cl::desc("Max instruction distance for spatial merging"),
    cl::init(500), cl::Hidden);

static cl::opt<bool> EnableSpatialClustering(
    "amdgpu-pre-ra-spatial-clustering",
    cl::desc("Enable spatial mega-clustering by program order"),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> SpatialPressureThreshold(
    "amdgpu-pre-ra-spatial-pressure",
    cl::desc("Max lanes per mega-cluster in spatial mode"),
    cl::init(200), cl::Hidden);

static cl::opt<bool> EnableAddrSplitting(
    "amdgpu-pre-ra-addr-split",
    cl::desc("Enable pre-RA address splitting (insert COPY + rewrite DS addr)"),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> AddrSplitLeadDistance(
    "amdgpu-pre-ra-addr-split-lead",
    cl::desc("How many real instructions before the first rewritten DS to place the COPY"),
    cl::init(0), cl::Hidden);

static cl::opt<unsigned> AddrSplitMinDSUses(
    "amdgpu-pre-ra-addr-split-min-ds",
    cl::desc("Minimum number of DS uses to rewrite for a split to be considered"),
    cl::init(4), cl::Hidden);

static cl::opt<unsigned> AddrSplitMinAvoided(
    "amdgpu-pre-ra-addr-split-min-avoided",
    cl::desc("Minimum predicted avoided exposed stalls per inserted COPY (conservative proxy)"),
    cl::init(2), cl::Hidden);

static cl::opt<unsigned> MaxHints(
    "amdgpu-pre-ra-max-hints",
    cl::desc("Maximum number of hints to emit per MachineFunction (0 = unlimited)"),
    cl::init(0), cl::Hidden);

static cl::opt<bool> DumpLanePressureTimeline(
    "amdgpu-pre-ra-dump-lane-pressure",
    cl::desc("Dump lane-weighted VGPR live pressure timeline per MBB (LiveIntervals + SlotIndexes)"),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> DumpLanePressureBucketSize(
    "amdgpu-pre-ra-dump-lane-pressure-bucket",
    cl::desc("Bucket size (in non-debug instructions) for lane pressure timeline dump"),
    cl::init(1), cl::Hidden);

static cl::opt<bool> DumpLanePressureLoopsOnly(
    "amdgpu-pre-ra-dump-lane-pressure-loops-only",
    cl::desc("Only dump lane pressure for MBBs that are inside a loop"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> EnablePreSplitRegionMode(
    "amdgpu-pre-ra-presplit-region",
    cl::desc("Assume the scheduler (or a pre-pass) has pre-split hot blocks into "
             "4 stripe subregions separated by SCHED_BARRIER 0; force region i "
             "to map to MSB block (i % 4) and avoid cross-block anchor bleed."),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> PreSplitSeedThreshold(
    "amdgpu-pre-ra-presplit-seed-threshold",
    cl::desc("Minimum regclass size (lanes) for presplit-region seeds"),
    cl::init(8), cl::Hidden);

static cl::opt<unsigned> PreSplitPKSeedMinLanes(
    "amdgpu-pre-ra-presplit-pk-seed-min-lanes",
    cl::desc("Minimum lane width for PK_VALU seeds in presplit-region mode. "
             "Default matches region slice threshold; setting to 2 can increase "
             "MSB coherence but may increase spills."),
    cl::init(8), cl::Hidden);

static cl::opt<unsigned> PreSplitPKPropagateMinLanes(
    "amdgpu-pre-ra-presplit-pk-propagate-min-lanes",
    cl::desc("Minimum lane width for PK_VALU propagation candidates in "
             "presplit-region mode. Default matches region slice threshold; setting to 2 "
             "can increase MSB coherence but may increase spills."),
    cl::init(8), cl::Hidden);

static cl::opt<bool> DumpPreSplitDetails(
    "amdgpu-pre-ra-presplit-dump",
    cl::desc("Dump presplit-region seeds, assignments, and block maps"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> DumpPreSplitQuality(
    "amdgpu-pre-ra-presplit-quality",
    cl::desc("Print one-line per-region presplit quality stats (hints/blocks)"),
    cl::init(false), cl::Hidden);

namespace {

/// Cluster of vregs that should prefer the same VGPR block.
struct MSBCluster {
  MachineInstr *DS = nullptr;
  MachineInstr *Next = nullptr;
  SmallVector<MachineInstr *, 8> DSReads;
  SmallVector<Register, 16> Members;
  SmallVector<Register, 8> Critical;    // DS dest, tied operands
  SmallVector<Register, 4> AddressRegs;

  int TargetBlock = -1;
  bool Allocated = false;
  unsigned WMMAWindowDepth = 0;         // 0=not in WMMA window, N=VALU slots
  unsigned TotalSizeInLanes = 0;
  unsigned FirstInstrIdx = UINT_MAX;
  unsigned LastInstrIdx = 0;
  unsigned ClusterID = 0;
  SmallVector<unsigned, 8> MergedFrom;

  bool isInWMMAWindow() const { return WMMAWindowDepth > 0; }

  unsigned spatialDistanceTo(const MSBCluster &Other) const {
    if (LastInstrIdx < Other.FirstInstrIdx)
      return Other.FirstInstrIdx - LastInstrIdx;
    if (Other.LastInstrIdx < FirstInstrIdx)
      return FirstInstrIdx - Other.LastInstrIdx;
    return 0;
  }

  std::string getName() const { return "C" + std::to_string(ClusterID); }

  std::string getMergedList() const {
    if (MergedFrom.empty())
      return getName();
    std::string Result = getName() + " [merged: ";
    for (size_t i = 0; i < MergedFrom.size(); ++i) {
      if (i > 0) Result += ", ";
      Result += "C" + std::to_string(MergedFrom[i]);
    }
    Result += "]";
    return Result;
  }

  void print(raw_ostream &OS, const TargetRegisterInfo *TRI) const {
    OS << getMergedList() << ": " << *DS;
    OS << "  -> Next: " << *Next;
    OS << "  Members (" << Members.size() << "): ";
    for (Register R : Members)
      OS << printReg(R, TRI) << " ";
    OS << "\n  Critical (" << Critical.size() << "): ";
    for (Register R : Critical)
      OS << printReg(R, TRI) << " ";
    if (!AddressRegs.empty()) {
      OS << "\n  AddressRegs (" << AddressRegs.size() << "): ";
      for (Register R : AddressRegs)
        OS << printReg(R, TRI) << " ";
    }
    OS << "\n  WMMAWindowDepth: " << WMMAWindowDepth;
    OS << ", TotalLanes: " << TotalSizeInLanes;
    OS << ", InstrRange: [" << FirstInstrIdx << "-" << LastInstrIdx << "]\n";
  }

  LLVM_DUMP_METHOD void dump(const TargetRegisterInfo *TRI) const {
    print(dbgs(), TRI);
  }
};

/// Region bounded by SCHED_BARRIER 0 markers.
struct SchedBarrierRegion {
  MachineBasicBlock::iterator Begin;
  MachineBasicBlock::iterator End;
  unsigned RegionIndex = 0;

  /// Instruction count within region (for def-order ranking).
  unsigned size() const {
    unsigned Count = 0;
    for (auto I = Begin; I != End; ++I)
      ++Count;
    return Count;
  }
};

/// Block assignment for a vreg in region-slicing mode.
struct RegionBlockAssignment {
  int Block = -1;
  enum Strength { None, Weak, Strong } AssignStrength = None;

  bool isAssigned() const { return Block >= 0; }
};

/// Seed vreg for region-slicing (wide tuple defs).
struct RegionSeed {
  Register Reg;
  unsigned SizeInLanes = 0;
  unsigned FirstDefIdx = UINT_MAX;
  bool IsStrongAnchor = false; // DS/WMMA dst
  StringRef Family;            // "WMMA", "DS", "PK_VALU", "EXP", etc.
  const MachineInstr *DefMI = nullptr;
};

class AMDGPUPreRAAlloc : public MachineFunctionPass {
public:
  static char ID;

  AMDGPUPreRAAlloc() : MachineFunctionPass(ID) {
    initializeAMDGPUPreRAAllocPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "AMDGPU Pre-RA Mini Allocator";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addRequired<SlotIndexesWrapperPass>();
    AU.addRequired<VirtRegMapWrapperLegacy>();
    AU.addRequired<LiveRegMatrixWrapperLegacy>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  LiveIntervals *LIS = nullptr;
  SlotIndexes *Indexes = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  const SIRegisterInfo *TRI = nullptr;
  const SIInstrInfo *TII = nullptr;
  const GCNSubtarget *ST = nullptr;
  MachineFunction *CurMF = nullptr;

  VirtRegMap *VRM = nullptr;
  LiveRegMatrix *Matrix = nullptr;
  MachineLoopInfo *MLI = nullptr;
  unsigned BlockPressure[4] = {0, 0, 0, 0};
  unsigned BlockPeakReg[4] = {0, 0, 0, 0};
  DenseMap<Register, int> AnchorBlock;
  DenseSet<Register> GlobalMustConstrain;

  struct AddrSplitRequest {
    Register OldAddr;
    unsigned TargetBlock = 0;
    MachineBasicBlock *MBB = nullptr;
    SmallVector<MachineInstr *, 8> DSUses;
  };

  DenseMap<MachineBasicBlock *, SmallVector<AddrSplitRequest, 4>> AddrSplitsByMBB;
  DenseSet<Register> AddrSplitBail;

  bool isDSRead(const MachineInstr &MI) const;
  bool isVALU(const MachineInstr &MI) const;
  bool isWMMA(const MachineInstr &MI) const;
  bool isHighFanout(Register R) const;
  Register getDSAddrVReg(const MachineInstr &MI) const;
  bool rewriteDSAddr(MachineInstr &MI, Register NewAddr) const;
  void collectMustConstrainForCluster(const MSBCluster &C,
                                      DenseSet<Register> &Out) const;
  int getAnchoredBlock(Register VReg) const;
  void recordAddrSplitRequests(ArrayRef<MSBCluster *> Batch);
  bool applyAddrSplits(SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);
  void allocateDSReadPairs(SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);

  void collectVGPRsFromInstr(const MachineInstr &MI,
                             SmallVectorImpl<Register> &Members,
                             SmallVectorImpl<Register> &Critical,
                             unsigned &TotalSize,
                             bool MarkDefsAsCritical,
                             bool MarkTiedAsCritical);
  void buildMegaClusters(SmallVectorImpl<MSBCluster> &Clusters,
                         DenseMap<Register, unsigned> &VRegToClusterIdx);
  void sortClustersByPriority(SmallVectorImpl<MSBCluster *> &Clusters);
  int getBatchPreferredBlock(ArrayRef<MSBCluster *> Batch);

  struct BatchResult {
    SmallVector<std::pair<Register, MCPhysReg>, 32> Allocated;
    SmallVector<std::pair<Register, unsigned>, 8> Failed; // (vreg, size in lanes)
    unsigned LargestFailedSize = 0;

    bool hasLargeTupleFailure(unsigned Threshold = 8) const {
      return LargestFailedSize > Threshold;
    }
  };

  /// Info about a cluster member for allocation ordering
  struct MemberInfo {
    Register Reg;
    unsigned SizeInLanes;
    bool IsCritical;
  };

  bool hasCriticalFailure(ArrayRef<MSBCluster *> Batch, const BatchResult &Result);
  bool tryAllocateWithShrink(SmallVectorImpl<MSBCluster *> &Batch,
                             unsigned TargetBlock, BatchResult &Result);
  void commitBatch(ArrayRef<MSBCluster *> Batch, const BatchResult &Result,
                   SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);
  void handleLeftoverClusters(ArrayRef<MSBCluster *> Clusters, size_t StartIdx,
                              SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);
  void allocateBatchesWithRetry(SmallVectorImpl<MSBCluster *> &Clusters,
                                SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);
  size_t formNextBatch(ArrayRef<MSBCluster *> Clusters, size_t StartIdx,
                       SmallVectorImpl<MSBCluster *> &Batch, unsigned &BatchSize);

  bool selectTargetBlock(MSBCluster &C);
  void allocateClusterMembers(MSBCluster &C,
                              SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated);
  MCPhysReg tryAllocateToBlock(Register VReg, unsigned Block);

  void dumpLanePressureTimelineForMBB(const MachineBasicBlock &MBB) const;
  void rollbackAllocations(ArrayRef<std::pair<Register, MCPhysReg>> ToRollback,
                           unsigned Block);
  BatchResult tryAllocateBatch(ArrayRef<MSBCluster *> Batch, unsigned Block);
  ArrayRef<MCPhysReg> getOrder(const TargetRegisterClass *RC) const;
  unsigned getBlockForPhysReg(MCPhysReg PhysReg) const;
  unsigned getRegSizeInLanes(Register VReg) const;
  unsigned getLeastPressuredBlock() const;
  bool blockHasCapacity(unsigned Block, unsigned LargestTuple) const;
  unsigned getWMMAWindowDepth(const MachineInstr &MI) const;
  void printPatternAnalysis(ArrayRef<MSBCluster> Clusters);

  // === Pre-split region mode methods ===
  bool runRegionBlockSlicing(MachineFunction &MF);
  bool hasSchedBarrier0(const MachineBasicBlock &MBB) const;
  SmallVector<SchedBarrierRegion, 4>
  splitBySchedBarrier0(MachineBasicBlock &MBB) const;
  SmallVector<RegionSeed, 32>
  collectRegionSeeds(const SchedBarrierRegion &Region) const;
  StringRef classifyOpcodeFamily(const MachineInstr &MI) const;
  bool isWideTupleVGPR(Register Reg) const;
  void assignBlocksToSeeds(ArrayRef<RegionSeed> Seeds,
                           DenseMap<Register, RegionBlockAssignment> &Assignments,
                           unsigned RegionLaneBudget[4],
                           unsigned ForcedBlock);
  void propagateOpcodeAware(const SchedBarrierRegion &Region,
                            DenseMap<Register, RegionBlockAssignment> &Assignments);
  unsigned emitRegionHints(
      const DenseMap<Register, RegionBlockAssignment> &Assignments,
      unsigned &HintsEmitted);
  void dumpRegionSeeds(ArrayRef<RegionSeed> Seeds,
                       const DenseMap<Register, RegionBlockAssignment> &Assignments) const;
  void dumpRegionAssignments(
      const DenseMap<Register, RegionBlockAssignment> &Assignments) const;
  void printPostAnalysis(ArrayRef<std::pair<Register, MCPhysReg>> Allocated) const;
  void analyzeMSBExposures() const;
};

} // end anonymous namespace

char AMDGPUPreRAAlloc::ID = 0;

INITIALIZE_PASS_BEGIN(AMDGPUPreRAAlloc, DEBUG_TYPE,
                      "AMDGPU Pre-RA Mini Allocator", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(SlotIndexesWrapperPass)
INITIALIZE_PASS_DEPENDENCY(VirtRegMapWrapperLegacy)
INITIALIZE_PASS_DEPENDENCY(LiveRegMatrixWrapperLegacy)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(AMDGPUPreRAAlloc, DEBUG_TYPE,
                    "AMDGPU Pre-RA Mini Allocator", false, false)

bool AMDGPUPreRAAlloc::isDSRead(const MachineInstr &MI) const {
  return SIInstrInfo::isDS(MI) && MI.mayLoad();
}

Register AMDGPUPreRAAlloc::getDSAddrVReg(const MachineInstr &MI) const {
  if (!isDSRead(MI))
    return Register();

  for (const MachineOperand &MO : MI.uses()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    const TargetRegisterClass *RC = MRI->getRegClass(MO.getReg());
    if (!TRI->isVGPRClass(RC))
      continue;
    return MO.getReg();
  }
  return Register();
}

bool AMDGPUPreRAAlloc::rewriteDSAddr(MachineInstr &MI, Register NewAddr) const {
  if (!isDSRead(MI))
    return false;
  if (!NewAddr || !NewAddr.isVirtual())
    return false;

  // Best-effort: rewrite the first virtual VGPR use operand (addr).
  for (MachineOperand &MO : MI.uses()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    const TargetRegisterClass *RC = MRI->getRegClass(MO.getReg());
    if (!TRI->isVGPRClass(RC))
      continue;
    MO.setReg(NewAddr);
    return true;
  }
  return false;
}

void AMDGPUPreRAAlloc::collectMustConstrainForCluster(
    const MSBCluster &C, DenseSet<Register> &Out) const {
  for (MachineInstr *DS : C.DSReads) {
    if (!DS)
      continue;
    // DS defs (dest tuple components)
    for (const MachineOperand &MO : DS->explicit_operands()) {
      if (!MO.isReg() || !MO.isDef() || !MO.getReg().isVirtual())
        continue;
      const TargetRegisterClass *RC = MRI->getRegClass(MO.getReg());
      if (!TRI->isVGPRClass(RC))
        continue;
      Out.insert(MO.getReg());
    }
    // DS addr vreg
    Register Addr = getDSAddrVReg(*DS);
    if (Addr)
      Out.insert(Addr);
  }
}

int AMDGPUPreRAAlloc::getAnchoredBlock(Register VReg) const {
  if (!VReg)
    return -1;
  if (VRM && VRM->hasPhys(VReg))
    return getBlockForPhysReg(VRM->getPhys(VReg));
  auto It = AnchorBlock.find(VReg);
  if (It != AnchorBlock.end())
    return It->second;
  return -1;
}

bool AMDGPUPreRAAlloc::isVALU(const MachineInstr &MI) const {
  return SIInstrInfo::isVALU(MI);
}

bool AMDGPUPreRAAlloc::isWMMA(const MachineInstr &MI) const {
  return SIInstrInfo::isWMMA(MI);
}

bool AMDGPUPreRAAlloc::isHighFanout(Register R) const {
  if (!R.isVirtual())
    return false;
  unsigned Count = 0;
  for ([[maybe_unused]] const MachineInstr &Use : MRI->use_nodbg_instructions(R)) {
    if (++Count > HighFanoutThreshold)
      return true;
  }
  return false;
}

MCPhysReg AMDGPUPreRAAlloc::tryAllocateToBlock(Register VReg, unsigned Block) {
  if (!VReg.isVirtual())
    return MCPhysReg();
  if (VRM->hasPhys(VReg))
    return VRM->getPhys(VReg);

  if (!LIS->hasInterval(VReg))
    return MCPhysReg();

  LiveInterval &LI = LIS->getInterval(VReg);
  if (LI.empty())
    return MCPhysReg();

  const TargetRegisterClass *RC = MRI->getRegClass(VReg);
  unsigned BlockStart = Block * 256;
  unsigned BlockEnd = BlockStart + 256;

  for (MCPhysReg PhysReg : getOrder(RC)) {
    unsigned HWReg = TRI->getHWRegIndex(PhysReg);
    if (HWReg < BlockStart || HWReg >= BlockEnd)
      continue;
    if (MRI->isReserved(PhysReg))
      continue;
    if (Matrix->checkInterference(LI, PhysReg) == LiveRegMatrix::IK_Free) {
      Matrix->assign(LI, PhysReg);
      unsigned SizeInLanes = getRegSizeInLanes(VReg);
      BlockPressure[Block] += SizeInLanes;
      unsigned RelativeEnd = HWReg + SizeInLanes - 1 - BlockStart;
      if (RelativeEnd > BlockPeakReg[Block])
        BlockPeakReg[Block] = RelativeEnd;
      return PhysReg;
    }
  }
  return MCPhysReg();
}

void AMDGPUPreRAAlloc::rollbackAllocations(
    ArrayRef<std::pair<Register, MCPhysReg>> ToRollback, unsigned Block) {
  LLVM_DEBUG(dbgs() << "  ROLLBACK: " << ToRollback.size() << " vregs\n");
  for (auto &[VReg, PhysReg] : ToRollback) {
    if (!LIS->hasInterval(VReg))
      continue;
    Matrix->unassign(LIS->getInterval(VReg));
    unsigned SizeInLanes = getRegSizeInLanes(VReg);
    if (BlockPressure[Block] >= SizeInLanes)
      BlockPressure[Block] -= SizeInLanes;
    LLVM_DEBUG(dbgs() << "    " << printReg(VReg, TRI) << "\n");
  }
}

AMDGPUPreRAAlloc::BatchResult
AMDGPUPreRAAlloc::tryAllocateBatch(ArrayRef<MSBCluster *> Batch,
                                   unsigned Block) {
  BatchResult Result;
  SmallVector<MemberInfo, 64> AllMembers;
  DenseSet<Register> Seen;

  DenseSet<Register> BatchMustConstrain;
  for (MSBCluster *C : Batch)
    collectMustConstrainForCluster(*C, BatchMustConstrain);

  for (MSBCluster *C : Batch) {
    C->TargetBlock = Block;
    for (Register R : BatchMustConstrain)
      if (!VRM->hasPhys(R) && !AnchorBlock.count(R))
        AnchorBlock[R] = Block;

    for (Register R : C->Critical) {
      if (VRM->hasPhys(R) || Seen.count(R))
        continue;
      Seen.insert(R);
      AllMembers.push_back({R, getRegSizeInLanes(R), true});
    }
    for (Register R : C->Members) {
      if (VRM->hasPhys(R) || Seen.count(R))
        continue;
      Seen.insert(R);
      AllMembers.push_back({R, getRegSizeInLanes(R), is_contained(C->Critical, R)});
    }
  }

  llvm::stable_sort(AllMembers, [](const MemberInfo &A, const MemberInfo &B) {
    if (A.IsCritical != B.IsCritical)
      return A.IsCritical > B.IsCritical;
    return A.SizeInLanes > B.SizeInLanes;
  });

  LLVM_DEBUG(dbgs() << "  Batch: " << AllMembers.size() << " vregs\n");

  bool CriticalFailed = false;
  for (const MemberInfo &M : AllMembers) {
    if (CriticalFailed) {
      Result.Failed.push_back({M.Reg, M.SizeInLanes});
      if (M.SizeInLanes > Result.LargestFailedSize)
        Result.LargestFailedSize = M.SizeInLanes;
      continue;
    }

    MCPhysReg PhysReg = tryAllocateToBlock(M.Reg, Block);
    if (PhysReg) {
      Result.Allocated.push_back({M.Reg, PhysReg});
    } else {
      Result.Failed.push_back({M.Reg, M.SizeInLanes});
      if (M.SizeInLanes > Result.LargestFailedSize)
        Result.LargestFailedSize = M.SizeInLanes;
      if (M.IsCritical)
        CriticalFailed = true;
    }
  }

  LLVM_DEBUG(dbgs() << "  Result: " << Result.Allocated.size() << " ok, "
                    << Result.Failed.size() << " failed\n");
  return Result;
}

void AMDGPUPreRAAlloc::collectVGPRsFromInstr(const MachineInstr &MI,
                                             SmallVectorImpl<Register> &Members,
                                             SmallVectorImpl<Register> &Critical,
                                             unsigned &TotalSize,
                                             bool MarkDefsAsCritical,
                                             bool MarkTiedAsCritical) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    const TargetRegisterClass *RC = MRI->getRegClass(MO.getReg());
    if (!TRI->isVGPRClass(RC))
      continue;

    Register R = MO.getReg();
    if (!is_contained(Members, R)) {
      Members.push_back(R);
      TotalSize += getRegSizeInLanes(R);
    }

    // Mark as critical based on caller's criteria
    bool IsCritical = (MarkDefsAsCritical && MO.isDef()) ||
                      (MarkTiedAsCritical && MO.isTied());
    if (IsCritical && !is_contained(Critical, R))
      Critical.push_back(R);
  }
}

void AMDGPUPreRAAlloc::buildMegaClusters(
    SmallVectorImpl<MSBCluster> &Clusters,
    DenseMap<Register, unsigned> &VRegToClusterIdx) {
  unsigned InstrIdx = 0;
  unsigned NextClusterID = 0;

  for (MachineBasicBlock &MBB : *CurMF) {
    if (SeedOnlyFromLoops && !MLI->getLoopFor(&MBB))
      continue;

    for (MachineInstr &MI : MBB) {
      ++InstrIdx;
      if (!isDSRead(MI))
        continue;

      MachineInstr *NextMI = SIInstrInfo::getNextRealInstr(&MI);
      if (!NextMI)
        continue;

      SmallVector<Register, 16> NewMembers;
      SmallVector<Register, 8> NewCritical;
      SmallVector<Register, 4> NewAddressRegs;
      unsigned NewSize = 0;

      collectVGPRsFromInstr(MI, NewMembers, NewCritical, NewSize, true, false);

      for (const MachineOperand &MO : MI.uses()) {
        if (MO.isReg() && MO.getReg().isVirtual()) {
          const TargetRegisterClass *RC = MRI->getRegClass(MO.getReg());
          if (TRI->isVGPRClass(RC)) {
            Register AddrReg = MO.getReg();
            if (!is_contained(NewCritical, AddrReg))
              NewCritical.push_back(AddrReg);
            if (!is_contained(NewAddressRegs, AddrReg))
              NewAddressRegs.push_back(AddrReg);
            break;
          }
        }
      }

      collectVGPRsFromInstr(*NextMI, NewMembers, NewCritical, NewSize, false, true);

      if (NewMembers.empty())
        continue;

      unsigned NewWMMADepth = getWMMAWindowDepth(MI);
      unsigned NewClusterID = NextClusterID++;

      MSBCluster *ExistingCluster = nullptr;
      for (Register R : NewMembers) {
        if (!EnableSpatialClustering && isHighFanout(R))
          continue;

        auto It = VRegToClusterIdx.find(R);
        if (It != VRegToClusterIdx.end()) {
          MSBCluster *Candidate = &Clusters[It->second];

          if (EnableSpatialClustering && SpatialWindowSize > 0) {
            unsigned Distance = (InstrIdx > Candidate->LastInstrIdx)
                ? (InstrIdx - Candidate->LastInstrIdx)
                : (Candidate->FirstInstrIdx - InstrIdx);
            if (Distance > SpatialWindowSize)
              continue;
            if (Candidate->TotalSizeInLanes + NewSize > SpatialPressureThreshold)
              continue;
          }
          ExistingCluster = Candidate;
          break;
        }
      }

      if (ExistingCluster) {
        ExistingCluster->MergedFrom.push_back(NewClusterID);
        ExistingCluster->DSReads.push_back(&MI);
        for (Register R : NewMembers) {
          if (!is_contained(ExistingCluster->Members, R)) {
            ExistingCluster->Members.push_back(R);
            ExistingCluster->TotalSizeInLanes += getRegSizeInLanes(R);
            if (EnableSpatialClustering || !isHighFanout(R))
              VRegToClusterIdx[R] = ExistingCluster - &Clusters[0];
          }
        }
        for (Register R : NewCritical)
          if (!is_contained(ExistingCluster->Critical, R))
            ExistingCluster->Critical.push_back(R);
        for (Register R : NewAddressRegs)
          if (!is_contained(ExistingCluster->AddressRegs, R))
            ExistingCluster->AddressRegs.push_back(R);
        if (NewWMMADepth > ExistingCluster->WMMAWindowDepth)
          ExistingCluster->WMMAWindowDepth = NewWMMADepth;
        ExistingCluster->FirstInstrIdx = std::min(ExistingCluster->FirstInstrIdx, InstrIdx);
        ExistingCluster->LastInstrIdx = std::max(ExistingCluster->LastInstrIdx, InstrIdx);
      } else {
        unsigned ClusterIdx = Clusters.size();
        Clusters.emplace_back();
        MSBCluster &C = Clusters.back();
        C.DS = &MI;
        C.Next = NextMI;
        C.DSReads.push_back(&MI);
        C.Members = std::move(NewMembers);
        C.Critical = std::move(NewCritical);
        C.AddressRegs = std::move(NewAddressRegs);
        C.TotalSizeInLanes = NewSize;
        C.WMMAWindowDepth = NewWMMADepth;
        C.FirstInstrIdx = InstrIdx;
        C.LastInstrIdx = InstrIdx;
        C.ClusterID = NewClusterID;
        for (Register R : C.Members)
          if (EnableSpatialClustering || !isHighFanout(R))
            VRegToClusterIdx[R] = ClusterIdx;
        LLVM_DEBUG(dbgs() << "  New cluster C" << NewClusterID << " ("
                          << C.TotalSizeInLanes << " lanes)\n");
      }
    }
  }
}

void AMDGPUPreRAAlloc::sortClustersByPriority(
    SmallVectorImpl<MSBCluster *> &Clusters) {
  if (EnableSpatialClustering) {
    llvm::stable_sort(Clusters, [](const MSBCluster *A, const MSBCluster *B) {
      if (A->FirstInstrIdx != B->FirstInstrIdx)
        return A->FirstInstrIdx < B->FirstInstrIdx;
      if (A->WMMAWindowDepth != B->WMMAWindowDepth)
        return A->WMMAWindowDepth > B->WMMAWindowDepth;
      return A->TotalSizeInLanes > B->TotalSizeInLanes;
    });
  } else {
    llvm::stable_sort(Clusters, [](const MSBCluster *A, const MSBCluster *B) {
      if (A->WMMAWindowDepth != B->WMMAWindowDepth)
        return A->WMMAWindowDepth > B->WMMAWindowDepth;
      return A->TotalSizeInLanes > B->TotalSizeInLanes;
    });
  }
}

size_t AMDGPUPreRAAlloc::formNextBatch(ArrayRef<MSBCluster *> Clusters,
                                       size_t StartIdx,
                                       SmallVectorImpl<MSBCluster *> &Batch,
                                       unsigned &BatchSize) {
  Batch.clear();
  BatchSize = 0;
  size_t Idx = StartIdx;
  DenseSet<Register> UniqueVRegs;

  while (Idx < Clusters.size()) {
    MSBCluster *C = Clusters[Idx];
    if (C->Allocated) {
      Idx++;
      continue;
    }
    if (WMMAWindowOnlyMode && C->WMMAWindowDepth == 0) {
      C->Allocated = true;
      Idx++;
      continue;
    }

    unsigned NewLanes = 0;
    for (Register R : C->Members)
      if (UniqueVRegs.insert(R).second)
        NewLanes += getRegSizeInLanes(R);

    if (BatchSize + NewLanes > BatchSizeTarget && !Batch.empty())
      break;

    Batch.push_back(C);
    BatchSize += NewLanes;
    Idx++;

    if (Batch.size() >= BatchMaxClusters)
      break;
  }
  return Idx - StartIdx;
}

int AMDGPUPreRAAlloc::getBatchPreferredBlock(ArrayRef<MSBCluster *> Batch) {
  int Votes[4] = {0, 0, 0, 0};

  for (MSBCluster *C : Batch) {
    if (C->Next) {
      for (const MachineOperand &MO : C->Next->operands()) {
        if (!MO.isReg()) continue;
        Register R = MO.getReg();
        if (R.isVirtual() && VRM->hasPhys(R))
          Votes[getBlockForPhysReg(VRM->getPhys(R))] += 100;
        else if (R.isPhysical() && TRI->isVGPR(*MRI, R))
          Votes[getBlockForPhysReg(R)] += 100;
      }
    }
    for (Register R : C->Members) {
      auto It = AnchorBlock.find(R);
      if (It != AnchorBlock.end())
        Votes[It->second] += 50;
    }
    if (C->DS) {
      for (const MachineOperand &MO : C->DS->operands()) {
        if (!MO.isReg() || MO.isDef()) continue;
        Register R = MO.getReg();
        if (R.isVirtual() && VRM->hasPhys(R))
          Votes[getBlockForPhysReg(VRM->getPhys(R))] += 10;
      }
    }
  }

  int BestBlock = -1, MaxVote = 0;
  for (int i = 0; i < 4; i++) {
    if (Votes[i] > MaxVote) {
      MaxVote = Votes[i];
      BestBlock = i;
    }
  }
  return BestBlock;
}

bool AMDGPUPreRAAlloc::hasCriticalFailure(ArrayRef<MSBCluster *> Batch,
                                          const BatchResult &Result) {
  for (const auto &Fail : Result.Failed) {
    for (const MSBCluster *C : Batch) {
      if (is_contained(C->Critical, Fail.first))
        return true;
    }
  }
  return false;
}

bool AMDGPUPreRAAlloc::tryAllocateWithShrink(SmallVectorImpl<MSBCluster *> &Batch,
                                             unsigned TargetBlock,
                                             BatchResult &Result) {
  LLVM_DEBUG(dbgs() << "\n  === Trying batch of " << Batch.size()
                    << " clusters in block " << TargetBlock << " ===\n");

  Result = tryAllocateBatch(Batch, TargetBlock);

  // Check if allocation is acceptable
  bool Failed = hasCriticalFailure(Batch, Result) ||
                Result.hasLargeTupleFailure(LargeTupleThreshold);

  if (!Failed)
    return true;  // Success on first try

  // Shrink and retry loop
  rollbackAllocations(Result.Allocated, TargetBlock);

  while (Batch.size() > 1) {
    Batch.pop_back();
    LLVM_DEBUG(dbgs() << "  Retrying with " << Batch.size() << " clusters\n");

    Result = tryAllocateBatch(Batch, TargetBlock);
    Failed = hasCriticalFailure(Batch, Result) ||
             Result.hasLargeTupleFailure(LargeTupleThreshold);

    if (!Failed)
      return true;  // Success after shrinking

    rollbackAllocations(Result.Allocated, TargetBlock);
  }

  // Even single cluster failed
  return false;
}

void AMDGPUPreRAAlloc::commitBatch(
    ArrayRef<MSBCluster *> Batch,
    const BatchResult &Result,
    SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  Allocated.append(Result.Allocated.begin(), Result.Allocated.end());
  for (MSBCluster *C : Batch)
    C->Allocated = true;
  if (EnableAddrSplitting)
    recordAddrSplitRequests(Batch);
  for (MSBCluster *C : Batch)
    collectMustConstrainForCluster(*C, GlobalMustConstrain);
}

void AMDGPUPreRAAlloc::handleLeftoverClusters(
    ArrayRef<MSBCluster *> Clusters,
    size_t StartIdx,
    SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  for (size_t i = StartIdx; i < Clusters.size(); i++) {
    MSBCluster *C = Clusters[i];
    if (C->Allocated)
      continue;
    if (WMMAWindowOnlyMode && C->WMMAWindowDepth == 0) {
      C->Allocated = true;
    } else if (selectTargetBlock(*C)) {
      allocateClusterMembers(*C, Allocated);
      C->Allocated = true;
      if (EnableAddrSplitting)
        recordAddrSplitRequests({C});
      collectMustConstrainForCluster(*C, GlobalMustConstrain);
    }
  }
}

void AMDGPUPreRAAlloc::allocateBatchesWithRetry(
    SmallVectorImpl<MSBCluster *> &RemainingClusters,
    SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  unsigned CurrentSequentialBlock = 0;
  size_t ClusterIdx = 0;

  while (ClusterIdx < RemainingClusters.size()) {
    SmallVector<MSBCluster *, 8> Batch;
    unsigned BatchSize = 0;
    size_t BatchStart = ClusterIdx;
    formNextBatch(RemainingClusters, ClusterIdx, Batch, BatchSize);

    if (Batch.empty()) {
      ClusterIdx++;
      continue;
    }

    int TargetBlock = getBatchPreferredBlock(Batch);
    bool UsedSequential = (TargetBlock == -1);
    if (UsedSequential)
      TargetBlock = CurrentSequentialBlock;

    BatchResult Result;
    bool Success = tryAllocateWithShrink(Batch, TargetBlock, Result);

    if (!Success) {
      if (!UsedSequential) {
        for (unsigned FallbackBlock = 0; FallbackBlock < 4; FallbackBlock++) {
          if (FallbackBlock == (unsigned)TargetBlock)
            continue;
          Success = tryAllocateWithShrink(Batch, FallbackBlock, Result);
          if (Success) {
            TargetBlock = FallbackBlock;
            break;
          }
        }
      } else if (CurrentSequentialBlock < 3) {
        CurrentSequentialBlock++;
        continue;
      }
      if (!Success) {
        ClusterIdx = BatchStart + 1;
        continue;
      }
    }

    commitBatch(Batch, Result, Allocated);
    ClusterIdx = BatchStart + Batch.size();

    while (CurrentSequentialBlock < 3 &&
           BlockPeakReg[CurrentSequentialBlock] >= 240)
      CurrentSequentialBlock++;
  }

  handleLeftoverClusters(RemainingClusters, ClusterIdx, Allocated);
}

void AMDGPUPreRAAlloc::recordAddrSplitRequests(ArrayRef<MSBCluster *> Batch) {
  for (MSBCluster *C : Batch) {
    if (!C || C->TargetBlock < 0)
      continue;
    unsigned TargetBlock = (unsigned)C->TargetBlock;

    for (Register Addr : C->AddressRegs) {
      if (!Addr || AddrSplitBail.count(Addr))
        continue;
      int Anchored = getAnchoredBlock(Addr);
      if (Anchored < 0 || (unsigned)Anchored == TargetBlock)
        continue;

      SmallBitVector Blocks(4);
      for (auto &KV : AddrSplitsByMBB)
        for (const AddrSplitRequest &Req : KV.second)
          if (Req.OldAddr == Addr && Req.TargetBlock < 4)
            Blocks.set(Req.TargetBlock);
      Blocks.set(TargetBlock);
      if (Blocks.count() > 2) {
        AddrSplitBail.insert(Addr);
        continue;
      }

      for (MachineInstr *DS : C->DSReads) {
        if (!DS || DS->getParent() == nullptr)
          continue;
        MachineBasicBlock *MBB = DS->getParent();
        if (getDSAddrVReg(*DS) != Addr)
          continue;

        auto &Vec = AddrSplitsByMBB[MBB];
        AddrSplitRequest *Existing = nullptr;
        for (auto &Req : Vec)
          if (Req.OldAddr == Addr && Req.TargetBlock == TargetBlock) {
            Existing = &Req;
            break;
          }
        if (!Existing) {
          Vec.push_back(AddrSplitRequest{Addr, TargetBlock, MBB, {}});
          Existing = &Vec.back();
        }
        if (!is_contained(Existing->DSUses, DS))
          Existing->DSUses.push_back(DS);
      }
    }
  }
}

bool AMDGPUPreRAAlloc::applyAddrSplits(
    SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  bool MadeChanges = false;

  for (auto &KV : AddrSplitsByMBB) {
    MachineBasicBlock *MBB = KV.first;
    if (!MBB)
      continue;

    for (AddrSplitRequest &Req : KV.second) {
      if (!Req.OldAddr || AddrSplitBail.count(Req.OldAddr))
        continue;
      if (Req.TargetBlock >= 4)
        continue;
      if (Req.DSUses.size() < AddrSplitMinDSUses)
        continue;
      if (Req.DSUses.size() < AddrSplitMinAvoided)
        continue;

      // Find the first DS use in this MBB.
      // MachineInstr does not provide a stable ordering comparator across all
      // LLVM versions, so scan the block in order.
      DenseSet<MachineInstr *> DSSet;
      for (MachineInstr *DS : Req.DSUses)
        if (DS && DS->getParent() == MBB)
          DSSet.insert(DS);

      MachineInstr *FirstUse = nullptr;
      for (MachineInstr &MI : *MBB) {
        if (DSSet.count(&MI)) {
          FirstUse = &MI;
          break;
        }
      }
      if (!FirstUse)
        continue;

      const TargetRegisterClass *RC = MRI->getRegClass(Req.OldAddr);
      Register NewAddr = MRI->createVirtualRegister(RC);

      MachineBasicBlock::iterator InsertIt = FirstUse->getIterator();
      unsigned Lead = AddrSplitLeadDistance;
      while (Lead > 0 && InsertIt != MBB->begin()) {
        auto Prev = std::prev(InsertIt);
        if (Prev->isDebugInstr() || Prev->isMetaInstruction()) {
          InsertIt = Prev;
          continue;
        }
        if (Prev->isTerminator())
          break;
        InsertIt = Prev;
        --Lead;
      }

      MachineInstr *CopyMI = BuildMI(*MBB, InsertIt, FirstUse->getDebugLoc(),
                                     TII->get(TargetOpcode::COPY), NewAddr)
                                 .addReg(Req.OldAddr);
      LIS->InsertMachineInstrInMaps(*CopyMI);

      bool RewroteAny = false;
      for (MachineInstr *DS : Req.DSUses) {
        if (!DS || DS->getParent() != MBB)
          continue;
        if (rewriteDSAddr(*DS, NewAddr))
          RewroteAny = true;
      }

      if (!RewroteAny) {
        LIS->RemoveMachineInstrFromMaps(*CopyMI);
        CopyMI->eraseFromParent();
        continue;
      }

      LIS->createAndComputeVirtRegInterval(NewAddr);
      if (LIS->hasInterval(Req.OldAddr))
        LIS->shrinkToUses(&LIS->getInterval(Req.OldAddr));

      MCPhysReg HintPhys = AMDGPU::VGPR0 + 256 * Req.TargetBlock;
      Allocated.push_back({NewAddr, HintPhys});
      GlobalMustConstrain.insert(NewAddr);
      MadeChanges = true;
    }
  }
  return MadeChanges;
}

void AMDGPUPreRAAlloc::printPatternAnalysis(ArrayRef<MSBCluster> Clusters) {
  if (Clusters.empty()) {
    dbgs() << "\n  === Pattern Analysis: No clusters ===\n";
    return;
  }

  dbgs() << "\n  === DS_READ Pattern Analysis ===\n";

  // 1. Next instruction type distribution
  unsigned NextIsVALU = 0, NextIsWMMA = 0, NextIsDSRead = 0, NextIsOther = 0;
  unsigned InWMMAWindow = 0, OutsideWMMAWindow = 0;

  // 2. Register reuse analysis
  DenseMap<Register, unsigned> AddrUseCounts;   // How many DS_READs use each address
  DenseMap<Register, unsigned> DstUseCounts;    // How many NextMIs use each DS dest
  DenseSet<Register> AllMembers;

  // 3. Cluster size distribution
  unsigned TotalLanes = 0;
  unsigned MinSize = UINT_MAX, MaxSize = 0;
  SmallVector<unsigned, 32> ClusterSizes;

  // 4. Critical register analysis
  unsigned TotalCritical = 0;

  for (const MSBCluster &C : Clusters) {
    // Size stats
    ClusterSizes.push_back(C.TotalSizeInLanes);
    TotalLanes += C.TotalSizeInLanes;
    MinSize = std::min(MinSize, C.TotalSizeInLanes);
    MaxSize = std::max(MaxSize, C.TotalSizeInLanes);
    TotalCritical += C.Critical.size();

    // WMMA window stats
    if (C.WMMAWindowDepth > 0)
      InWMMAWindow++;
    else
      OutsideWMMAWindow++;

    // Collect all members for reuse analysis
    for (Register R : C.Members)
      AllMembers.insert(R);

    // Next instruction type
    if (C.Next) {
      if (isWMMA(*C.Next))
        NextIsWMMA++;
      else if (isVALU(*C.Next))
        NextIsVALU++;
      else if (isDSRead(*C.Next))
        NextIsDSRead++;
      else
        NextIsOther++;
    }

    // Address register analysis (for DS_READ inputs)
    for (MachineInstr *DS : C.DSReads) {
      if (!DS)
        continue;
      // Track the first virtual VGPR use (addr), consistent with cluster builder.
      for (const MachineOperand &MO : DS->uses()) {
        if (!MO.isReg() || !MO.getReg().isVirtual())
          continue;
        if (!TRI->isVGPRClass(MRI->getRegClass(MO.getReg())))
          continue;
        AddrUseCounts[MO.getReg()]++;
        break;
      }
    }
  }

  // Compute address fanout distribution
  unsigned HighFanoutAddrs = 0;
  unsigned MaxAddrFanout = 0;
  Register MostSharedAddr;
  for (auto &KV : AddrUseCounts) {
    if (KV.second > MaxAddrFanout) {
      MaxAddrFanout = KV.second;
      MostSharedAddr = KV.first;
    }
    if (KV.second > HighFanoutThreshold)
      HighFanoutAddrs++;
  }

  // Print results
  dbgs() << "\n  --- Cluster Statistics ---\n";
  dbgs() << "  Total clusters: " << Clusters.size() << "\n";
  dbgs() << "  Total lanes: " << TotalLanes << "\n";
  dbgs() << "  Size range: [" << MinSize << ", " << MaxSize << "]\n";
  dbgs() << "  Average size: " << (TotalLanes / Clusters.size()) << " lanes\n";
  dbgs() << "  Total critical regs: " << TotalCritical << "\n";
  dbgs() << "  Unique vregs across all clusters: " << AllMembers.size() << "\n";

  dbgs() << "\n  --- WMMA Window Distribution ---\n";
  dbgs() << "  In WMMA window: " << InWMMAWindow
         << " (" << (100 * InWMMAWindow / Clusters.size()) << "%)\n";
  dbgs() << "  Outside WMMA window: " << OutsideWMMAWindow
         << " (" << (100 * OutsideWMMAWindow / Clusters.size()) << "%)\n";

  dbgs() << "\n  --- Next Instruction Types ---\n";
  dbgs() << "  VALU: " << NextIsVALU << "\n";
  dbgs() << "  WMMA: " << NextIsWMMA << "\n";
  dbgs() << "  DS_READ: " << NextIsDSRead << "\n";
  dbgs() << "  Other: " << NextIsOther << "\n";

  dbgs() << "\n  --- Address Register Fanout ---\n";
  dbgs() << "  Unique address regs: " << AddrUseCounts.size() << "\n";
  dbgs() << "  High-fanout addresses (>" << HighFanoutThreshold << " uses): "
         << HighFanoutAddrs << "\n";
  if (MostSharedAddr)
    dbgs() << "  Most shared address: " << printReg(MostSharedAddr, TRI)
           << " (" << MaxAddrFanout << " uses)\n";

  // Estimate block requirements
  unsigned EstBlocksNeeded = (TotalLanes + 255) / 256;
  dbgs() << "\n  --- Block Estimation ---\n";
  dbgs() << "  Estimated blocks needed: " << EstBlocksNeeded
         << " (if perfectly packed)\n";
  dbgs() << "  Actual block capacity: 4 blocks × 256 regs = 1024 regs\n";

  // Cluster connectivity analysis (how many clusters share vregs)
  dbgs() << "\n  --- Cluster Connectivity ---\n";
  unsigned SharedMemberCount = 0;
  DenseMap<Register, unsigned> MemberClusterCount;
  for (const MSBCluster &C : Clusters) {
    for (Register R : C.Members)
      MemberClusterCount[R]++;
  }
  for (auto &KV : MemberClusterCount) {
    if (KV.second > 1)
      SharedMemberCount++;
  }
  dbgs() << "  Vregs shared across multiple clusters: " << SharedMemberCount << "\n";
  if (SharedMemberCount > 0) {
    // Find most shared
    Register MostShared;
    unsigned MostSharedCount = 0;
    for (auto &KV : MemberClusterCount) {
      if (KV.second > MostSharedCount) {
        MostSharedCount = KV.second;
        MostShared = KV.first;
      }
    }
    dbgs() << "  Most connected vreg: " << printReg(MostShared, TRI)
           << " (in " << MostSharedCount << " clusters)\n";
  }

  dbgs() << "\n";
}

void AMDGPUPreRAAlloc::allocateDSReadPairs(
    SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  DenseMap<Register, unsigned> VRegToClusterIdx;
  SmallVector<MSBCluster, 32> Clusters;
  buildMegaClusters(Clusters, VRegToClusterIdx);

  LLVM_DEBUG(dbgs() << "  Built " << Clusters.size() << " clusters\n");

  SmallVector<MSBCluster *, 32> SortedClusters;
  SortedClusters.reserve(Clusters.size());
  for (MSBCluster &C : Clusters)
    SortedClusters.push_back(&C);
  sortClustersByPriority(SortedClusters);

  allocateBatchesWithRetry(SortedClusters, Allocated);

  LLVM_DEBUG(dbgs() << "  Allocated: " << Allocated.size() << " vregs\n");
}

bool AMDGPUPreRAAlloc::selectTargetBlock(MSBCluster &C) {
  unsigned LargestTuple = 0;
  for (Register R : C.Members)
    LargestTuple = std::max(LargestTuple, getRegSizeInLanes(R));

  int CandidateBlock = -1;

  for (Register R : C.Members) {
    auto It = AnchorBlock.find(R);
    if (It != AnchorBlock.end()) {
      CandidateBlock = It->second;
      if (blockHasCapacity(CandidateBlock, LargestTuple)) {
        C.TargetBlock = CandidateBlock;
        return true;
      }
      break;
    }
  }

  for (const MachineOperand &MO : C.Next->operands()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    if (VRM->hasPhys(MO.getReg())) {
      CandidateBlock = getBlockForPhysReg(VRM->getPhys(MO.getReg()));
      if (blockHasCapacity(CandidateBlock, LargestTuple)) {
        C.TargetBlock = CandidateBlock;
        return true;
      }
      break;
    }
  }

  for (const MachineOperand &MO : C.DS->operands()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    if (VRM->hasPhys(MO.getReg())) {
      CandidateBlock = getBlockForPhysReg(VRM->getPhys(MO.getReg()));
      if (blockHasCapacity(CandidateBlock, LargestTuple)) {
        C.TargetBlock = CandidateBlock;
        return true;
      }
      break;
    }
  }

  for (const MachineOperand &MO : C.Next->operands()) {
    if (!MO.isReg() || !MO.getReg().isPhysical())
      continue;
    if (TRI->isVGPR(*MRI, MO.getReg())) {
      CandidateBlock = getBlockForPhysReg(MO.getReg());
      if (blockHasCapacity(CandidateBlock, LargestTuple)) {
        C.TargetBlock = CandidateBlock;
        return true;
      }
      break;
    }
  }

  if (AnchorOnlyMode)
    return false;

  if (DefaultBlock < 4 && blockHasCapacity(DefaultBlock, LargestTuple))
    C.TargetBlock = DefaultBlock;
  else
    C.TargetBlock = getLeastPressuredBlock();
  return true;
}

void AMDGPUPreRAAlloc::allocateClusterMembers(
    MSBCluster &C, SmallVectorImpl<std::pair<Register, MCPhysReg>> &Allocated) {
  for (Register R : C.Members)
    if (!VRM->hasPhys(R) && !AnchorBlock.count(R))
      AnchorBlock[R] = C.TargetBlock;

  auto SortBySize = [this](Register A, Register B) {
    return getRegSizeInLanes(A) > getRegSizeInLanes(B);
  };
  llvm::stable_sort(C.Critical, SortBySize);
  llvm::stable_sort(C.Members, SortBySize);

  for (Register R : C.Critical) {
    if (VRM->hasPhys(R))
      continue;
    if (MCPhysReg PhysReg = tryAllocateToBlock(R, C.TargetBlock))
      Allocated.push_back({R, PhysReg});
  }

  for (Register R : C.Members) {
    if (VRM->hasPhys(R) || is_contained(C.Critical, R))
      continue;
    if (MCPhysReg PhysReg = tryAllocateToBlock(R, C.TargetBlock))
      Allocated.push_back({R, PhysReg});
  }
}

ArrayRef<MCPhysReg>
AMDGPUPreRAAlloc::getOrder(const TargetRegisterClass *RC) const {
  return RC->getRawAllocationOrder(*CurMF);
}

unsigned AMDGPUPreRAAlloc::getBlockForPhysReg(MCPhysReg PhysReg) const {
  unsigned HWReg = TRI->getHWRegIndex(PhysReg);
  return HWReg / 256;
}

unsigned AMDGPUPreRAAlloc::getLeastPressuredBlock() const {
  // Peak-centric: prefer block with lowest peak (most headroom for tuples).
  // This is more accurate than pressure because compact packing means
  // many vregs can share the same physical registers.
  unsigned MinBlock = 0;
  unsigned MinPeak = BlockPeakReg[0];
  for (unsigned i = 1; i < 4; i++) {
    if (BlockPeakReg[i] < MinPeak) {
      MinPeak = BlockPeakReg[i];
      MinBlock = i;
    }
  }
  return MinBlock;
}

bool AMDGPUPreRAAlloc::blockHasCapacity(unsigned Block,
                                        unsigned LargestTuple) const {
  constexpr unsigned PeakThreshold = 240;
  if (Block >= 4)
    return false;
  unsigned Headroom = 255 - BlockPeakReg[Block];
  return Headroom >= LargestTuple && BlockPeakReg[Block] < PeakThreshold;
}

unsigned AMDGPUPreRAAlloc::getRegSizeInLanes(Register VReg) const {
  if (!VReg.isVirtual())
    return 1;
  const TargetRegisterClass *RC = MRI->getRegClass(VReg);
  // Size in bits / 32 = number of 32-bit lanes
  return TRI->getRegSizeInBits(*RC) / 32;
}

unsigned AMDGPUPreRAAlloc::getWMMAWindowDepth(const MachineInstr &MI) const {
  const MachineBasicBlock *MBB = MI.getParent();
  constexpr unsigned WindowSize = 8;

  MachineBasicBlock::const_iterator It = MI.getIterator();
  unsigned DistanceToWMMA = 0;
  bool FoundPrecedingWMMA = false;

  for (unsigned i = 0; i < WindowSize && It != MBB->begin(); i++) {
    --It;
    if (It->isDebugInstr() || It->isMetaInstruction())
      continue;
    DistanceToWMMA++;
    if (isWMMA(*It)) {
      FoundPrecedingWMMA = true;
      break;
    }
  }

  if (!FoundPrecedingWMMA || DistanceToWMMA >= WindowSize)
    return 0;

  unsigned RemainingWindow = WindowSize - DistanceToWMMA;
  It = MI.getIterator();
  unsigned VALUCount = 0, SlotsCounted = 0;

  while (SlotsCounted < RemainingWindow) {
    ++It;
    if (It == MBB->end())
      break;
    if (It->isDebugInstr() || It->isMetaInstruction())
      continue;
    SlotsCounted++;
    if (isWMMA(*It))
      break;
    if (isVALU(*It))
      VALUCount++;
  }
  return VALUCount;
}

bool AMDGPUPreRAAlloc::runOnMachineFunction(MachineFunction &MF) {
  if (!EnablePreRAAlloc)
    return false;

  CurMF = &MF;
  ST = &MF.getSubtarget<GCNSubtarget>();
  if (!ST->hasGFX1250Insts())
    return false;

  TRI = ST->getRegisterInfo();
  TII = ST->getInstrInfo();
  MRI = &MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervalsWrapperPass>().getLIS();
  Indexes = &getAnalysis<SlotIndexesWrapperPass>().getSI();
  VRM = &getAnalysis<VirtRegMapWrapperLegacy>().getVRM();
  Matrix = &getAnalysis<LiveRegMatrixWrapperLegacy>().getLRM();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  LLVM_DEBUG(dbgs() << "AMDGPUPreRAAlloc: " << MF.getName() << "\n");

  AnchorBlock.clear();
  GlobalMustConstrain.clear();
  AddrSplitsByMBB.clear();
  AddrSplitBail.clear();
  std::fill(std::begin(BlockPressure), std::end(BlockPressure), 0);
  std::fill(std::begin(BlockPeakReg), std::end(BlockPeakReg), 0);

  // === Pre-split region mode (block-partitioned allocation) ===
  if (EnablePreSplitRegionMode) {
    bool Changed = runRegionBlockSlicing(MF);

    if (DumpLanePressureTimeline && Indexes && LIS) {
      dbgs() << "\n  === Lane-weighted VGPR live pressure timeline (pre-RA, SlotIndexes-based) ===\n";
      dbgs() << "  Note: timeline is by instruction order (not cycles). Bucket="
             << std::max(1u, (unsigned)DumpLanePressureBucketSize) << "\n";
      for (const MachineBasicBlock &MBB : MF) {
        if (DumpLanePressureLoopsOnly && !MLI->getLoopFor(&MBB))
          continue;
        dumpLanePressureTimelineForMBB(MBB);
      }
    }
    return Changed;
  }

  // === Original DS_READ clustering mode ===
  SmallVector<std::pair<Register, MCPhysReg>, 64> Allocated;
  allocateDSReadPairs(Allocated);

  if (Allocated.empty())
    return false;

  if (EnablePostAnalysis)
    LLVM_DEBUG(printPostAnalysis(Allocated));

  bool MadeChanges = false;
  if (EnableAddrSplitting)
    MadeChanges |= applyAddrSplits(Allocated);

  unsigned HintsEmitted = 0;
  for (auto &[VReg, PhysReg] : Allocated) {
    if (!GlobalMustConstrain.empty() && !GlobalMustConstrain.count(VReg))
      continue;
    if (MaxHints != 0 && HintsEmitted >= MaxHints)
      break;
    MRI->setRegAllocationHint(VReg, 0, PhysReg);
    ++HintsEmitted;
  }

  LLVM_DEBUG(dbgs() << "  Hints: " << HintsEmitted << "\n");

  if (DumpLanePressureTimeline && Indexes && LIS) {
    dbgs() << "\n  === Lane-weighted VGPR live pressure timeline (pre-RA, SlotIndexes-based) ===\n";
    dbgs() << "  Note: timeline is by instruction order (not cycles). Bucket="
           << std::max(1u, (unsigned)DumpLanePressureBucketSize) << "\n";
    for (const MachineBasicBlock &MBB : MF) {
      if (DumpLanePressureLoopsOnly && !MLI->getLoopFor(&MBB))
        continue;
      dumpLanePressureTimelineForMBB(MBB);
    }
  }
  return MadeChanges;
}

void AMDGPUPreRAAlloc::printPostAnalysis(
    ArrayRef<std::pair<Register, MCPhysReg>> Allocated) const {
  dbgs() << "\n  === Allocation Analysis ===\n";

  // Block distribution of allocations
  unsigned BlockCount[4] = {0, 0, 0, 0};
  for (const auto &[VReg, PhysReg] : Allocated) {
    unsigned HWReg = TRI->getHWRegIndex(PhysReg);
    unsigned Block = HWReg / 256;
    if (Block < 4)
      BlockCount[Block]++;
  }
  dbgs() << "  Allocated: " << Allocated.size() << " vregs\n";
  dbgs() << "  Block distribution: [0]=" << BlockCount[0]
         << " [1]=" << BlockCount[1] << " [2]=" << BlockCount[2]
         << " [3]=" << BlockCount[3] << "\n";
  dbgs() << "  Block pressure (lanes): [0]=" << BlockPressure[0]
         << " [1]=" << BlockPressure[1] << " [2]=" << BlockPressure[2]
         << " [3]=" << BlockPressure[3] << "\n";
  dbgs() << "  Block peak (highest reg): [0]=" << BlockPeakReg[0]
         << " [1]=" << BlockPeakReg[1] << " [2]=" << BlockPeakReg[2]
         << " [3]=" << BlockPeakReg[3] << "\n";
  if (!AnchorBlock.empty())
    dbgs() << "  Anchors propagated: " << AnchorBlock.size() << " vregs\n";

  // MSB Exposure Analysis
  analyzeMSBExposures();
}

void AMDGPUPreRAAlloc::analyzeMSBExposures() const {
  dbgs() << "\n  === MSB Exposure Analysis (DS_READ -> next instr) ===\n";

  struct ExposureInfo {
    const MachineInstr *DSRead;
    const MachineInstr *NextMI;
    int DstBlock;                      // DS_READ destination block
    SmallVector<int, 4> NextOpBlocks;  // Blocks of next instruction's operands
    bool IsExposed;                    // Would require set_msb
    bool InLoop;
  };

  SmallVector<ExposureInfo, 16> Exposures;
  unsigned TotalDSReads = 0;
  unsigned ExposedCount = 0;
  unsigned UnallocatedCount = 0;
  unsigned InLoopExposed = 0;

  // Pattern tracking: what kinds of block transitions are we seeing?
  unsigned BlockTransitions[4][4] = {}; // [dst_block][next_op_block]

  for (const MachineBasicBlock &MBB : *CurMF) {
    for (const MachineInstr &MI : MBB) {
      if (!isDSRead(MI))
        continue;

      TotalDSReads++;

      MachineInstr *NextMI = SIInstrInfo::getNextRealInstr(
          const_cast<MachineInstr *>(&MI));
      if (!NextMI)
        continue;

      ExposureInfo Info;
      Info.DSRead = &MI;
      Info.NextMI = NextMI;
      Info.IsExposed = false;
      Info.InLoop = MLI->getLoopFor(&MBB) != nullptr;
      Info.DstBlock = -1;

      // Get DS_READ destination block
      Register DstReg;
      for (const MachineOperand &MO : MI.explicit_operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isVirtual()) {
          DstReg = MO.getReg();
          break;
        }
      }

      if (DstReg && VRM->hasPhys(DstReg)) {
        Info.DstBlock = getBlockForPhysReg(VRM->getPhys(DstReg));
      } else if (DstReg) {
        // Check MRI hints (what we set)
        std::pair<Register, Register> Hint = MRI->getRegAllocationHint(DstReg);
        if (Hint.second && Hint.second.isPhysical())
          Info.DstBlock = getBlockForPhysReg(Hint.second);
        else {
          UnallocatedCount++;
          continue; // Can't analyze without allocation
        }
      } else {
        continue;
      }

      // Get next instruction's VGPR operand blocks
      for (const MachineOperand &MO : NextMI->operands()) {
        if (!MO.isReg() || !TRI->isVGPR(*MRI, MO.getReg()))
          continue;

        int OpBlock = -1;
        if (MO.getReg().isPhysical()) {
          OpBlock = getBlockForPhysReg(MO.getReg());
        } else if (VRM->hasPhys(MO.getReg())) {
          OpBlock = getBlockForPhysReg(VRM->getPhys(MO.getReg()));
        } else {
          // Check hint
          std::pair<Register, Register> Hint =
              MRI->getRegAllocationHint(MO.getReg());
          if (Hint.second && Hint.second.isPhysical())
            OpBlock = getBlockForPhysReg(Hint.second);
        }

        if (OpBlock >= 0) {
          Info.NextOpBlocks.push_back(OpBlock);
          if (OpBlock != Info.DstBlock) {
            Info.IsExposed = true;
            BlockTransitions[Info.DstBlock][OpBlock]++;
          }
        }
      }

      if (Info.IsExposed) {
        ExposedCount++;
        if (Info.InLoop)
          InLoopExposed++;
        Exposures.push_back(Info);
      }
    }
  }

  // Summary
  dbgs() << "  Total DS_READs: " << TotalDSReads << "\n";
  dbgs() << "  Unallocated (can't analyze): " << UnallocatedCount << "\n";
  dbgs() << "  MSB Exposed: " << ExposedCount;
  if (InLoopExposed > 0)
    dbgs() << " (" << InLoopExposed << " in loops - HIGH IMPACT)";
  dbgs() << "\n";

  // Block transition matrix
  dbgs() << "\n  Block transition matrix (dst -> next_op):\n";
  dbgs() << "         to: [0]   [1]   [2]   [3]\n";
  for (int d = 0; d < 4; d++) {
    dbgs() << "  from [" << d << "]: ";
    for (int n = 0; n < 4; n++) {
      dbgs() << format("%4u  ", BlockTransitions[d][n]);
    }
    dbgs() << "\n";
  }

  // Helper to print register mapping
  auto printRegMapping = [&](Register Reg) {
    if (!Reg.isValid())
      return;
    dbgs() << "        " << printReg(Reg, TRI);
    if (Reg.isPhysical()) {
      dbgs() << " -> PHYS (block " << getBlockForPhysReg(Reg) << ")\n";
    } else if (VRM->hasPhys(Reg)) {
      MCPhysReg Phys = VRM->getPhys(Reg);
      dbgs() << " -> " << printReg(Phys, TRI) << " (block "
             << getBlockForPhysReg(Phys) << ") [VRM]\n";
    } else {
      std::pair<Register, Register> Hint = MRI->getRegAllocationHint(Reg);
      if (Hint.second && Hint.second.isPhysical()) {
        dbgs() << " -> HINT " << printReg(Hint.second, TRI) << " (block "
               << getBlockForPhysReg(Hint.second) << ")\n";
      } else {
        dbgs() << " -> UNASSIGNED\n";
      }
    }
  };

  // Show individual exposures with FULL instructions
  if (!Exposures.empty()) {
    dbgs() << "\n  === Exposed DS_READ -> next_instr pairs ===\n";

    for (unsigned i = 0; i < Exposures.size(); i++) {
      const ExposureInfo &E = Exposures[i];
      dbgs() << "\n  [" << i << "] " << (E.InLoop ? "IN_LOOP" : "not_loop")
             << " | dst_block=" << E.DstBlock << " -> next_blocks={";
      for (size_t j = 0; j < E.NextOpBlocks.size(); j++) {
        if (j > 0) dbgs() << ",";
        int b = E.NextOpBlocks[j];
        if (b != E.DstBlock)
          dbgs() << b << "*";  // Mark conflicting block
        else
          dbgs() << b;
      }
      dbgs() << "}\n";
      dbgs() << "      DS_READ: " << *E.DSRead;
      dbgs() << "      NEXT:    " << *E.NextMI;

      // Print register mappings for DS_READ operands
      dbgs() << "      Mappings (DS_READ):\n";
      for (const MachineOperand &MO : E.DSRead->operands()) {
        if (MO.isReg() && MO.getReg().isValid())
          printRegMapping(MO.getReg());
      }

      // Print register mappings for next instruction operands
      dbgs() << "      Mappings (NEXT):\n";
      for (const MachineOperand &MO : E.NextMI->operands()) {
        if (MO.isReg() && MO.getReg().isValid())
          printRegMapping(MO.getReg());
      }
    }
  }

  // What's the dominant bad pattern?
  int MaxFrom = -1, MaxTo = -1;
  unsigned MaxCount = 0;
  for (int d = 0; d < 4; d++) {
    for (int n = 0; n < 4; n++) {
      if (d != n && BlockTransitions[d][n] > MaxCount) {
        MaxCount = BlockTransitions[d][n];
        MaxFrom = d;
        MaxTo = n;
      }
    }
  }

  if (MaxCount > 0) {
    dbgs() << "\n  Dominant exposure pattern: block " << MaxFrom << " -> block "
           << MaxTo << " (" << MaxCount << " occurrences)\n";
    dbgs() << "  Suggestion: Try -amdgpu-preallocator-default-ds-read-block="
           << MaxTo << " or improve anchoring\n";
  }
}

void AMDGPUPreRAAlloc::dumpLanePressureTimelineForMBB(
    const MachineBasicBlock &MBB) const {
  if (!Indexes || !LIS || !MRI || !TRI)
    return;

  const unsigned Bucket = std::max(1u, (unsigned)DumpLanePressureBucketSize);
  SlotIndex MBBStart = Indexes->getMBBStartIdx(&MBB);
  SlotIndex MBBEnd = Indexes->getMBBEndIdx(&MBB);

  SmallVector<const MachineInstr *, 128> Instrs;
  SmallVector<SlotIndex, 128> InstrIdx;
  Instrs.reserve(MBB.size());
  InstrIdx.reserve(MBB.size());

  for (const MachineInstr &MI : MBB) {
    if (MI.isDebugInstr() || MI.isMetaInstruction())
      continue;
    Instrs.push_back(&MI);
    InstrIdx.push_back(Indexes->getInstructionIndex(MI));
  }

  if (Instrs.empty())
    return;

  DenseSet<Register> VRegsInBlock;
  for (const MachineInstr *MI : Instrs) {
    for (const MachineOperand &MO : MI->operands()) {
      if (!MO.isReg())
        continue;
      Register R = MO.getReg();
      if (!R.isVirtual())
        continue;
      if (!TRI->isVGPR(*MRI, R))
        continue;
      VRegsInBlock.insert(R);
    }
  }

  struct Event {
    SlotIndex Idx;
    int Block; // 0..3, or 4=unknown
    int Delta; // +/- lanes
  };

  SmallVector<Event, 256> Events;
  Events.reserve(VRegsInBlock.size() * 2);

  for (Register R : VRegsInBlock) {
    if (!LIS->hasInterval(R))
      continue;
    const LiveInterval &LI = LIS->getInterval(R);
    unsigned Size = getRegSizeInLanes(R);
    if (Size == 0)
      continue;

    int B = getAnchoredBlock(R);
    if (B < 0 || B > 3)
      B = 4;

    // Add events for any segment that overlaps this MBB's slot range.
    for (const LiveRange::Segment &S : LI.segments) {
      SlotIndex A = std::max(S.start, MBBStart);
      SlotIndex E = std::min(S.end, MBBEnd);
      if (!(A < E))
        continue;
      Events.push_back({A, B, (int)Size});
      Events.push_back({E, B, -(int)Size});
    }
  }

  if (Events.empty())
    return;

  llvm::sort(Events, [](const Event &A, const Event &B) {
    if (A.Idx != B.Idx)
      return A.Idx < B.Idx;
    // Apply adds before removes at the same index for more intuitive "live-at" printing.
    return A.Delta > B.Delta;
  });

  dbgs() << "\n  -- MBB " << MBB.getNumber();
  if (MBB.getBasicBlock())
    dbgs() << " (" << MBB.getBasicBlock()->getName() << ")";
  dbgs() << " --\n";

  // Active lane pressure per predicted block (0..3) and unknown (4).
  int Active[5] = {0, 0, 0, 0, 0};
  unsigned EIdx = 0;

  auto applyEventsUpTo = [&](SlotIndex Idx) {
    while (EIdx < Events.size() && !(Idx < Events[EIdx].Idx)) {
      int B = Events[EIdx].Block;
      Active[B] += Events[EIdx].Delta;
      if (Active[B] < 0)
        Active[B] = 0; // Defensive against weird segments / rounding.
      ++EIdx;
    }
  };

  auto totalActive = [&]() -> int {
    return Active[0] + Active[1] + Active[2] + Active[3] + Active[4];
  };

  // Bucket the instruction stream and report peak pressure in each bucket.
  for (unsigned I = 0; I < Instrs.size(); I += Bucket) {
    int Peak[5] = {0, 0, 0, 0, 0};
    int PeakTotal = 0;

    unsigned EndI = std::min<unsigned>(Instrs.size(), I + Bucket);
    for (unsigned J = I; J < EndI; ++J) {
      applyEventsUpTo(InstrIdx[J]);
      int Tot = totalActive();
      PeakTotal = std::max(PeakTotal, Tot);
      for (int B = 0; B < 5; ++B)
        Peak[B] = std::max(Peak[B], Active[B]);
    }

    const MachineInstr *ShowMI = Instrs[I];
    dbgs() << "    [" << I << "-" << (EndI - 1) << "] "
           << "peak_lanes total=" << PeakTotal
           << " b0=" << Peak[0] << " b1=" << Peak[1] << " b2=" << Peak[2]
           << " b3=" << Peak[3] << " u=" << Peak[4] << " | ";
    ShowMI->print(dbgs());
  }
}

//===----------------------------------------------------------------------===//
// Region-slicing mode implementation
//===----------------------------------------------------------------------===//

bool AMDGPUPreRAAlloc::hasSchedBarrier0(const MachineBasicBlock &MBB) const {
  for (const MachineInstr &MI : MBB) {
    if (MI.getOpcode() == AMDGPU::SCHED_BARRIER &&
        MI.getOperand(0).getImm() == 0)
      return true;
  }
  return false;
}

SmallVector<SchedBarrierRegion, 4>
AMDGPUPreRAAlloc::splitBySchedBarrier0(MachineBasicBlock &MBB) const {
  SmallVector<SchedBarrierRegion, 4> Regions;
  auto Begin = MBB.begin();
  unsigned Idx = 0;

  for (auto I = MBB.begin(), E = MBB.end(); I != E; ++I) {
    if (I->getOpcode() == AMDGPU::SCHED_BARRIER &&
        I->getOperand(0).getImm() == 0) {
      SchedBarrierRegion R;
      R.Begin = Begin;
      R.End = I;
      R.RegionIndex = Idx++;
      if (R.Begin != R.End)
        Regions.push_back(R);
      Begin = std::next(I);
    }
  }

  // Final region after last barrier
  SchedBarrierRegion R;
  R.Begin = Begin;
  R.End = MBB.end();
  R.RegionIndex = Idx;
  if (R.Begin != R.End)
    Regions.push_back(R);

  return Regions;
}

StringRef AMDGPUPreRAAlloc::classifyOpcodeFamily(const MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  if (isWMMA(MI))
    return "WMMA";
  if (isDSRead(MI))
    return "DS";

  // V_EXP
  if (Opc == AMDGPU::V_EXP_F32_e32 || Opc == AMDGPU::V_EXP_F32_e64)
    return "EXP";

  // V_PK_* (packed VALU) - detected via VOP3P flag
  if (SIInstrInfo::isVOP3P(MI))
    return "PK_VALU";

  // Generic wide VALU (includes V_CVT and other ops)
  if (isVALU(MI))
    return "VALU";

  return "";
}

bool AMDGPUPreRAAlloc::isWideTupleVGPR(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  const TargetRegisterClass *RC = MRI->getRegClass(Reg);
  if (!TRI->hasVGPRs(RC))
    return false;
  unsigned Size = getRegSizeInLanes(Reg);
  return Size >= PreSplitSeedThreshold;
}

SmallVector<RegionSeed, 32>
AMDGPUPreRAAlloc::collectRegionSeeds(const SchedBarrierRegion &Region) const {
  SmallVector<RegionSeed, 32> Seeds;
  DenseSet<Register> SeenRegs;
  unsigned InstrIdx = 0;

  for (auto I = Region.Begin; I != Region.End; ++I, ++InstrIdx) {
    const MachineInstr &MI = *I;
    StringRef Family = classifyOpcodeFamily(MI);
    if (Family.empty())
      continue;

    bool IsStrongAnchor = (Family == "WMMA" || Family == "DS");

    // Collect wide VGPR defs
    for (const MachineOperand &MO : MI.defs()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (!Reg.isVirtual()) {
        if (DumpPreSplitDetails)
          dbgs() << "    skip " << printReg(Reg, TRI)
                 << ": non-virtual def\n";
        continue;
      }
      unsigned SeedMinLanes =
          (Family == "PK_VALU") ? PreSplitPKSeedMinLanes
                                : PreSplitSeedThreshold;
      unsigned SizeLanes = getRegSizeInLanes(Reg);
      if (SizeLanes < SeedMinLanes) {
        if (DumpPreSplitDetails)
          dbgs() << "    skip " << printReg(Reg, TRI)
                 << ": below seed threshold (" << SizeLanes << " lanes)\n";
        continue;
      }
      if (SeenRegs.count(Reg)) {
        if (DumpPreSplitDetails)
          dbgs() << "    skip " << printReg(Reg, TRI)
                 << ": already seeded in region\n";
        continue;
      }
      SeenRegs.insert(Reg);

      RegionSeed S;
      S.Reg = Reg;
      S.SizeInLanes = SizeLanes;
      S.FirstDefIdx = InstrIdx;
      S.IsStrongAnchor = IsStrongAnchor;
      S.Family = Family;
      S.DefMI = &MI;
      Seeds.push_back(S);
    }
  }

  // Sort by first def index
  llvm::sort(Seeds, [](const RegionSeed &A, const RegionSeed &B) {
    return A.FirstDefIdx < B.FirstDefIdx;
  });

  return Seeds;
}

void AMDGPUPreRAAlloc::assignBlocksToSeeds(
    ArrayRef<RegionSeed> Seeds,
    DenseMap<Register, RegionBlockAssignment> &Assignments,
    unsigned RegionLaneBudget[4],
    unsigned ForcedBlock) {
  for (unsigned I = 0; I < Seeds.size(); ++I) {
    const RegionSeed &S = Seeds[I];

    // Skip if already assigned (from global carryover)
    if (Assignments.count(S.Reg) && Assignments[S.Reg].isAssigned())
      continue;

    unsigned B = ForcedBlock;
    RegionBlockAssignment BA;
    BA.Block = B;
    BA.AssignStrength = S.IsStrongAnchor ? RegionBlockAssignment::Strong
                                         : RegionBlockAssignment::Weak;
    Assignments[S.Reg] = BA;
    RegionLaneBudget[B] += S.SizeInLanes;

    LLVM_DEBUG(dbgs() << "    Seed " << printReg(S.Reg, TRI)
                      << " (" << S.Family << ", " << S.SizeInLanes << " lanes)"
                      << " -> Block " << B << " (forced region)"
                      << (BA.AssignStrength == RegionBlockAssignment::Strong
                              ? " (strong)"
                              : "")
                      << "\n");
  }
}

void AMDGPUPreRAAlloc::propagateOpcodeAware(
    const SchedBarrierRegion &Region,
    DenseMap<Register, RegionBlockAssignment> &Assignments) {
  
  bool Changed = true;
  unsigned Iterations = 0;
  const unsigned MaxIterations = 10;

  while (Changed && Iterations++ < MaxIterations) {
    Changed = false;

    for (auto I = Region.Begin; I != Region.End; ++I) {
      const MachineInstr &MI = *I;
      StringRef Family = classifyOpcodeFamily(MI);
      if (Family.empty())
        continue;

      // Collect candidate regs for propagation (opcode-aware, wide tuples only).
      SmallVector<Register, 8> CandidateRegs;
      bool AllowSmall = (Family == "PK_VALU");
      auto addCandidate = [&](Register R) {
        if (!R.isVirtual())
          return;
        if (!AllowSmall && !isWideTupleVGPR(R))
          return;
        if (AllowSmall && getRegSizeInLanes(R) < PreSplitPKPropagateMinLanes)
          return;
        if (!is_contained(CandidateRegs, R))
          CandidateRegs.push_back(R);
      };

      if (Family == "DS") {
        for (const MachineOperand &MO : MI.defs())
          if (MO.isReg())
            addCandidate(MO.getReg());
      } else {
        for (const MachineOperand &MO : MI.defs())
          if (MO.isReg())
            addCandidate(MO.getReg());
        for (const MachineOperand &MO : MI.explicit_uses())
          if (MO.isReg())
            addCandidate(MO.getReg());
      }

      if (CandidateRegs.empty())
        continue;

      // Find dominant block from assigned candidates
      int DominantBlock = -1;
      RegionBlockAssignment::Strength DominantStrength =
          RegionBlockAssignment::None;

      for (Register R : CandidateRegs) {
        auto It = Assignments.find(R);
        if (It != Assignments.end() && It->second.isAssigned()) {
          if (It->second.AssignStrength > DominantStrength) {
            DominantBlock = It->second.Block;
            DominantStrength = It->second.AssignStrength;
          }
        }
      }

      if (DominantBlock < 0)
        continue;

      // Propagate to unassigned wide VGPRs only
      // Opcode-aware: only propagate along tile operands
      for (Register R : CandidateRegs) {
        if (Assignments.count(R) && Assignments[R].isAssigned())
          continue;

        RegionBlockAssignment BA;
        BA.Block = DominantBlock;
        BA.AssignStrength = RegionBlockAssignment::Weak;
        Assignments[R] = BA;
        Changed = true;

        LLVM_DEBUG(dbgs() << "    Propagate " << printReg(R, TRI)
                          << " -> Block " << DominantBlock << " (from "
                          << Family << ")\n");
      }
    }
  }
}

void AMDGPUPreRAAlloc::dumpRegionSeeds(
    ArrayRef<RegionSeed> Seeds,
    const DenseMap<Register, RegionBlockAssignment> &Assignments) const {
  dbgs() << "    Seed list:\n";
  for (const RegionSeed &S : Seeds) {
    int Block = -1;
    auto It = Assignments.find(S.Reg);
    if (It != Assignments.end() && It->second.isAssigned())
      Block = It->second.Block;
    dbgs() << "      " << printReg(S.Reg, TRI) << " (" << S.Family
           << ", " << S.SizeInLanes << " lanes, def@" << S.FirstDefIdx << ")";
    if (Block >= 0)
      dbgs() << " -> B" << Block;
    if (S.IsStrongAnchor)
      dbgs() << " [strong]";
    dbgs() << "\n";
  }
}

void AMDGPUPreRAAlloc::dumpRegionAssignments(
    const DenseMap<Register, RegionBlockAssignment> &Assignments) const {
  SmallVector<Register, 64> BlockRegs[4];
  SmallVector<Register, 64> Unassigned;

  for (const auto &[Reg, BA] : Assignments) {
    if (!BA.isAssigned() || BA.Block < 0 || BA.Block >= 4) {
      Unassigned.push_back(Reg);
      continue;
    }
    BlockRegs[BA.Block].push_back(Reg);
  }

  for (int B = 0; B < 4; ++B) {
    llvm::sort(BlockRegs[B], [](Register A, Register B) {
      return A.id() < B.id();
    });
    dbgs() << "    Block " << B << " vregs (" << BlockRegs[B].size()
           << "): ";
    for (Register R : BlockRegs[B])
      dbgs() << printReg(R, TRI) << " ";
    dbgs() << "\n";
  }

  dbgs() << "    Pre-RA block map (wide tuples only):\n";
  for (int B = 0; B < 4; ++B) {
    dbgs() << "      B" << B << ":";
    for (Register R : BlockRegs[B]) {
      if (!isWideTupleVGPR(R))
        continue;
      dbgs() << " " << printReg(R, TRI);
    }
    dbgs() << "\n";
  }

  if (!Unassigned.empty()) {
    llvm::sort(Unassigned, [](Register A, Register B) {
      return A.id() < B.id();
    });
    dbgs() << "    Unassigned vregs (" << Unassigned.size() << "): ";
    for (Register R : Unassigned)
      dbgs() << printReg(R, TRI) << " ";
    dbgs() << "\n";
  }
}

unsigned AMDGPUPreRAAlloc::emitRegionHints(
    const DenseMap<Register, RegionBlockAssignment> &Assignments,
    unsigned &HintsEmitted) {
  unsigned LocalHints = 0;

  for (const auto &[Reg, BA] : Assignments) {
    if (!BA.isAssigned())
      continue;

    if (BA.AssignStrength == RegionBlockAssignment::Strong)
      GlobalMustConstrain.insert(Reg);

    if (MaxHints != 0 && HintsEmitted >= MaxHints)
      break;

    // Use the new MSBBlock hint types
    unsigned HintType = AMDGPURI::getHintForMSBBlock(BA.Block);

    // Set a representative physreg in that block as the hint register
    // (The actual block preference comes from the hint type)
    MCPhysReg RepPhys = AMDGPU::VGPR0 + (BA.Block * 256);

    MRI->setRegAllocationHint(Reg, HintType, RepPhys);
    ++HintsEmitted;
    ++LocalHints;

    // Update AnchorBlock for carryover
    AnchorBlock[Reg] = BA.Block;
  }

  return LocalHints;
}

bool AMDGPUPreRAAlloc::runRegionBlockSlicing(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "  === PreSplit Region Mode ===\n");

  bool MadeChanges = false;
  unsigned TotalRegions = 0;
  unsigned TotalSeeds = 0;
  unsigned TotalAssigned = 0;
  unsigned HintsEmitted = 0;

  for (MachineBasicBlock &MBB : MF) {
    // Only process MBBs with SCHED_BARRIER 0 markers
    if (!hasSchedBarrier0(MBB))
      continue;

    // Only process loop bodies (hot code)
    if (SeedOnlyFromLoops && !MLI->getLoopFor(&MBB))
      continue;

    LLVM_DEBUG(dbgs() << "  Processing MBB " << MBB.getNumber() << "\n");

    SmallVector<SchedBarrierRegion, 4> Regions = splitBySchedBarrier0(MBB);
    LLVM_DEBUG(dbgs() << "    Found " << Regions.size() << " regions\n");

    for (const SchedBarrierRegion &Region : Regions) {
      LLVM_DEBUG(dbgs() << "  --- Region " << Region.RegionIndex << " ("
                        << Region.size() << " instrs) ---\n");
      TotalRegions++;

      unsigned ForcedBlock = (Region.RegionIndex % 4);

      // Collect seeds (wide tuple defs)
      SmallVector<RegionSeed, 32> Seeds = collectRegionSeeds(Region);
      TotalSeeds += Seeds.size();
      LLVM_DEBUG(dbgs() << "    Seeds: " << Seeds.size() << "\n");
      if (DumpPreSplitDetails) {
        dbgs() << "    PreSplit forced block: " << ForcedBlock << "\n";
        unsigned ExpCount = 0, PkCount = 0, ValuCount = 0, WmmaCount = 0,
                 DsCount = 0, OtherCount = 0;
        for (const RegionSeed &S : Seeds) {
          if (S.Family == "EXP")
            ++ExpCount;
          else if (S.Family == "PK_VALU")
            ++PkCount;
          else if (S.Family == "VALU")
            ++ValuCount;
          else if (S.Family == "WMMA")
            ++WmmaCount;
          else if (S.Family == "DS")
            ++DsCount;
          else
            ++OtherCount;
        }
        dbgs() << "    Region seeds: EXP=" << ExpCount
               << ", PK_VALU=" << PkCount << ", VALU=" << ValuCount
               << ", WMMA=" << WmmaCount << ", DS=" << DsCount;
        if (OtherCount)
          dbgs() << ", OTHER=" << OtherCount;
        dbgs() << "\n";
      }
      if (Seeds.empty())
        continue;

      // Initialize lane budget per block for this region
      unsigned RegionLaneBudget[4] = {0, 0, 0, 0};

      // Inherit global assignments for vregs used in this region
      DenseMap<Register, RegionBlockAssignment> Assignments;
      for (auto I = Region.Begin; I != Region.End; ++I) {
        for (const MachineOperand &MO : I->operands()) {
          if (!MO.isReg() || !MO.getReg().isVirtual())
            continue;
          Register R = MO.getReg();
          auto It = AnchorBlock.find(R);
          if (It != AnchorBlock.end() && It->second >= 0 && It->second < 4) {
            // In forced-block mode, avoid contaminating the region with
            // cross-block anchors. Only inherit matching anchors unless the
            // value is globally must-constrain.
            if (static_cast<unsigned>(It->second) != ForcedBlock &&
                !GlobalMustConstrain.count(R))
              continue;
            RegionBlockAssignment BA;
            BA.Block = It->second;
            BA.AssignStrength = GlobalMustConstrain.count(R)
                                    ? RegionBlockAssignment::Strong
                                    : RegionBlockAssignment::Weak;
            Assignments[R] = BA;
          }
        }
      }

      // Account for inherited assignments in the region budget.
      for (const auto &[Reg, BA] : Assignments) {
        if (!BA.isAssigned())
          continue;
        RegionLaneBudget[BA.Block] += getRegSizeInLanes(Reg);
      }

      // Assign blocks to seeds (forced region block).
      assignBlocksToSeeds(Seeds, Assignments, RegionLaneBudget, ForcedBlock);

      // Propagate through def-use chains (opcode-aware)
      propagateOpcodeAware(Region, Assignments);

      if (DumpPreSplitDetails) {
        dumpRegionSeeds(Seeds, Assignments);
        dumpRegionAssignments(Assignments);
      }

      // Emit hints using new MSBBlock hint types
      unsigned LocalHints = emitRegionHints(Assignments, HintsEmitted);

      TotalAssigned += Assignments.size();
      if (LocalHints > 0)
        MadeChanges = true;

      if (DumpPreSplitQuality) {
        unsigned BlockAssignedCounts[4] = {0, 0, 0, 0};
        unsigned BlockAssignedLanes[4] = {0, 0, 0, 0};
        unsigned Unassigned = 0;
        for (const auto &[Reg, BA] : Assignments) {
          if (!BA.isAssigned()) {
            ++Unassigned;
            continue;
          }
          if (BA.Block < 4) {
            ++BlockAssignedCounts[BA.Block];
            BlockAssignedLanes[BA.Block] += getRegSizeInLanes(Reg);
          }
        }
        errs() << "PreRARegionSlice: bb=" << MBB.getNumber()
               << " region=" << Region.RegionIndex
               << " instrs=" << Region.size()
               << " seeds=" << Seeds.size()
               << " assigned=" << Assignments.size()
               << " unassigned=" << Unassigned
               << " hints=" << LocalHints
               << " budget=["
               << RegionLaneBudget[0] << "," << RegionLaneBudget[1] << ","
               << RegionLaneBudget[2] << "," << RegionLaneBudget[3] << "]"
               << " assigned_lanes=["
               << BlockAssignedLanes[0] << "," << BlockAssignedLanes[1] << ","
               << BlockAssignedLanes[2] << "," << BlockAssignedLanes[3] << "]"
               << " assigned_regs=["
               << BlockAssignedCounts[0] << "," << BlockAssignedCounts[1] << ","
               << BlockAssignedCounts[2] << "," << BlockAssignedCounts[3] << "]"
               << "\n";
      }

      LLVM_DEBUG(dbgs() << "    Assigned: " << Assignments.size() << " vregs\n"
                        << "    Block budget: [0]=" << RegionLaneBudget[0]
                        << " [1]=" << RegionLaneBudget[1]
                        << " [2]=" << RegionLaneBudget[2]
                        << " [3]=" << RegionLaneBudget[3] << "\n");
    }
  }

  LLVM_DEBUG(dbgs() << "  Region slicing summary: " << TotalRegions
                    << " regions, " << TotalSeeds << " seeds, " << TotalAssigned
                    << " assigned, " << HintsEmitted << " hints\n");

  return MadeChanges;
}

FunctionPass *llvm::createAMDGPUPreRAAllocPass() {
  return new AMDGPUPreRAAlloc();
}
