//===-- AMDGPULDSPrefetch.cpp - Prefetch LDS loads one iteration ahead ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass implements software pipelining of LDS loads within loops.
// It ensures WMMA instructions use data that was prefetched in the previous
// iteration, hiding LDS load latency.
//
// The key transformation creates PHI nodes that delay the use of LDS load
// results by one iteration:
//
//   Before:
//     loop:
//       %data = load LDS
//       use %data in WMMA  <-- immediate use
//
//   After:
//     preheader:
//       %init = load from initial LDS location
//     loop:
//       %phi = phi [%init, preheader], [%data, latch]
//       %data = load LDS  <-- prefetch for next iteration
//       use %phi in WMMA  <-- uses previous iteration's data
//
// Handles pointer-swapping double-buffer patterns where LDS base pointers
// rotate between iterations via PHI nodes.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-lds-prefetch"

static cl::opt<bool> EnableLDSPrefetch(
    "amdgpu-enable-lds-prefetch", cl::init(false), cl::Hidden,
    cl::desc("Enable LDS prefetching one iteration ahead in loops"));

static cl::opt<unsigned> LDSPrefetchMinLoads(
    "amdgpu-lds-prefetch-min-loads", cl::init(4), cl::Hidden,
    cl::desc("Minimum number of LDS loads in loop to enable prefetching"));

static cl::opt<unsigned> LDSPrefetchMaxLoads(
    "amdgpu-lds-prefetch-max-loads", cl::init(0), cl::Hidden,
    cl::desc("Maximum number of LDS loads to prefetch (0 = unlimited)"));

STATISTIC(NumLoopsPrefetched, "Number of loops with LDS prefetching applied");
STATISTIC(NumLoadsPrefetched, "Number of LDS loads prefetched");

namespace {

class AMDGPULDSPrefetch : public FunctionPass {
public:
  static char ID;

  AMDGPULDSPrefetch() : FunctionPass(ID) {
    initializeAMDGPULDSPrefetchPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
  }

  StringRef getPassName() const override { return "AMDGPU LDS Prefetch"; }

private:
  bool processLoop(Loop *L, LoopInfo &LI, ScalarEvolution &SE,
                   DominatorTree &DT);
  bool isLDSLoad(Instruction *I) const;
  bool feedsWMMA(Instruction *I, unsigned Depth = 0) const;

  void collectLDSLoads(Loop *L, SmallVectorImpl<LoadInst *> &Loads) const;

  Value *cloneAddressForPreheader(Value *Ptr, Loop *L, Instruction *InsertPt,
                                   DenseMap<Value *, Value *> &CloneMap) const;
};

} // end anonymous namespace

char AMDGPULDSPrefetch::ID = 0;

INITIALIZE_PASS_BEGIN(AMDGPULDSPrefetch, DEBUG_TYPE,
                      "AMDGPU LDS Prefetch", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(AMDGPULDSPrefetch, DEBUG_TYPE,
                    "AMDGPU LDS Prefetch", false, false)

bool AMDGPULDSPrefetch::isLDSLoad(Instruction *I) const {
  if (auto *LI = dyn_cast<LoadInst>(I)) {
    unsigned AS = LI->getPointerAddressSpace();
    return AS == 3;
  }
  return false;
}

// Check if a load's result feeds into WMMA (directly or through shuffles/bitcasts)
bool AMDGPULDSPrefetch::feedsWMMA(Instruction *I, unsigned Depth) const {
  if (Depth > 10)
    return false;

  for (User *U : I->users()) {
    if (auto *CI = dyn_cast<CallInst>(U)) {
      if (Function *F = CI->getCalledFunction()) {
        StringRef Name = F->getName();
        if (Name.contains("wmma"))
          return true;
      }
    }
    if (auto *SV = dyn_cast<ShuffleVectorInst>(U)) {
      if (feedsWMMA(SV, Depth + 1))
        return true;
    }
    if (auto *BC = dyn_cast<BitCastInst>(U)) {
      if (feedsWMMA(BC, Depth + 1))
        return true;
    }
    if (auto *IE = dyn_cast<InsertElementInst>(U)) {
      if (feedsWMMA(IE, Depth + 1))
        return true;
    }
  }
  return false;
}

void AMDGPULDSPrefetch::collectLDSLoads(Loop *L,
                                         SmallVectorImpl<LoadInst *> &Loads) const {
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getPointerAddressSpace() == 3) {
          // Only transform loads that feed WMMA to minimize PHI count
          if (feedsWMMA(LI)) {
            Loads.push_back(LI);
          }
        }
      }
    }
  }
}

// Clone an address computation for use in the preheader.
// For PHI nodes, use the preheader incoming value.
// For other instructions, clone them.
Value *AMDGPULDSPrefetch::cloneAddressForPreheader(Value *Ptr, Loop *L,
                                                    Instruction *InsertPt,
                                                    DenseMap<Value *, Value *> &CloneMap) const {
  if (auto It = CloneMap.find(Ptr); It != CloneMap.end())
    return It->second;

  // If loop-invariant, use as-is
  if (L->isLoopInvariant(Ptr)) {
    CloneMap[Ptr] = Ptr;
    return Ptr;
  }

  auto *Inst = dyn_cast<Instruction>(Ptr);
  if (!Inst) {
    CloneMap[Ptr] = Ptr;
    return Ptr;
  }

  // For PHI nodes in the header, use the preheader incoming value
  if (auto *PHI = dyn_cast<PHINode>(Inst)) {
    if (PHI->getParent() == L->getHeader()) {
      BasicBlock *Preheader = L->getLoopPreheader();
      if (Preheader) {
        for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
          if (PHI->getIncomingBlock(i) == Preheader) {
            Value *InitVal = PHI->getIncomingValue(i);
            CloneMap[Ptr] = InitVal;
            return InitVal;
          }
        }
      }
    }
  }

  // For other instructions in the loop, clone them
  if (L->contains(Inst)) {
    Instruction *Clone = Inst->clone();
    Clone->setName(Inst->getName() + ".init");
    Clone->insertBefore(InsertPt->getIterator());

    // Recursively clone operands
    for (unsigned i = 0; i < Clone->getNumOperands(); ++i) {
      Value *Op = Clone->getOperand(i);
      Value *NewOp = cloneAddressForPreheader(Op, L, Clone, CloneMap);
      Clone->setOperand(i, NewOp);
    }

    CloneMap[Ptr] = Clone;
    return Clone;
  }

  // Instruction is outside the loop, use as-is
  CloneMap[Ptr] = Ptr;
  return Ptr;
}

bool AMDGPULDSPrefetch::processLoop(Loop *L, LoopInfo &LI, ScalarEvolution &SE,
                                     DominatorTree &DT) {
  if (!L->isInnermost())
    return false;

  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *Latch = L->getLoopLatch();

  if (!Preheader || !Header || !Latch)
    return false;

  LLVM_DEBUG(dbgs() << "Processing loop with header " << Header->getName() << "\n");

  SmallVector<LoadInst *, 32> LDSLoads;
  collectLDSLoads(L, LDSLoads);

  if (LDSLoads.size() < LDSPrefetchMinLoads) {
    LLVM_DEBUG(dbgs() << "Only " << LDSLoads.size()
                      << " WMMA-feeding LDS loads, need " << LDSPrefetchMinLoads << "\n");
    return false;
  }

  LLVM_DEBUG(dbgs() << "Found " << LDSLoads.size() << " WMMA-feeding LDS loads\n");

  bool Changed = false;
  Instruction *PreheaderInsertPt = Preheader->getTerminator();

  unsigned NumPrefetched = 0;
  unsigned MaxLoads = LDSPrefetchMaxLoads > 0 ? LDSPrefetchMaxLoads : LDSLoads.size();

  for (LoadInst *Load : LDSLoads) {
    if (NumPrefetched >= MaxLoads) {
      LLVM_DEBUG(dbgs() << "Reached max prefetch limit (" << MaxLoads << ")\n");
      break;
    }
    Value *Ptr = Load->getPointerOperand();

    LLVM_DEBUG(dbgs() << "Processing load: " << *Load << "\n"
                      << "  Ptr: " << *Ptr << "\n");

    // Clone the address computation for the preheader
    // This replaces loop PHIs with their preheader incoming values
    DenseMap<Value *, Value *> CloneMap;
    Value *InitPtr = cloneAddressForPreheader(Ptr, L, PreheaderInsertPt, CloneMap);

    LLVM_DEBUG(dbgs() << "  InitPtr: " << *InitPtr << "\n");

    // Create the initial load in the preheader
    LoadInst *InitLoad = new LoadInst(Load->getType(), InitPtr,
                                       Load->getName() + ".init",
                                       PreheaderInsertPt->getIterator());
    InitLoad->setAlignment(Load->getAlign());
    InitLoad->setVolatile(Load->isVolatile());

    LLVM_DEBUG(dbgs() << "  Created init load: " << *InitLoad << "\n");

    // Create PHI node at the start of the header
    PHINode *DataPHI = PHINode::Create(Load->getType(), 2,
                                        Load->getName() + ".prefetch.phi");
    DataPHI->insertBefore(Header->begin());

    // Add incoming values:
    // - From preheader: use the init load (iteration 0's data from buffer 0)
    // - From loop: use THIS load (which loaded from the buffer that was
    //              just filled in the previous iteration)
    DataPHI->addIncoming(InitLoad, Preheader);

    for (BasicBlock *Pred : predecessors(Header)) {
      if (Pred != Preheader && L->contains(Pred)) {
        DataPHI->addIncoming(Load, Pred);
      }
    }

    // Replace uses of the load with the PHI
    // EXCEPT: the PHI itself should not be replaced
    SmallVector<Use *, 16> UsesToReplace;
    for (Use &U : Load->uses()) {
      auto *User = cast<Instruction>(U.getUser());
      if (User == DataPHI)
        continue;
      if (!L->contains(User))
        continue;
      UsesToReplace.push_back(&U);
    }

    for (Use *U : UsesToReplace)
      U->set(DataPHI);

    ++NumLoadsPrefetched;
    ++NumPrefetched;
    Changed = true;

    LLVM_DEBUG(dbgs() << "  Created prefetch PHI: " << *DataPHI << "\n");
  }

  if (Changed)
    ++NumLoopsPrefetched;

  return Changed;
}

bool AMDGPULDSPrefetch::runOnFunction(Function &F) {
  if (!EnableLDSPrefetch)
    return false;

  if (F.getCallingConv() != CallingConv::AMDGPU_KERNEL)
    return false;

  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  bool Changed = false;

  SmallVector<Loop *, 8> Worklist;
  for (Loop *L : LI)
    for (Loop *InnerL : depth_first(L))
      Worklist.push_back(InnerL);

  for (Loop *L : Worklist) {
    Changed |= processLoop(L, LI, SE, DT);
  }

  return Changed;
}

char &llvm::AMDGPULDSPrefetchID = AMDGPULDSPrefetch::ID;

FunctionPass *llvm::createAMDGPULDSPrefetchPass() {
  return new AMDGPULDSPrefetch();
}
