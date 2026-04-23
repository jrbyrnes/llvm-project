//===- RevectorizeExtractPhi.cpp - Re-vectorize extracted scalar phis ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass finds patterns where scalar values extracted from vectors flow
// through phi nodes, and transforms them to keep the values as vectors.
//
// Pattern before:
//   ; In preheader or loop exit:
//   %e0 = extractelement <4 x i8> %vec, i64 0
//   %e1 = extractelement <4 x i8> %vec, i64 1
//   %e2 = extractelement <4 x i8> %vec, i64 2
//   %e3 = extractelement <4 x i8> %vec, i64 3
//   br label %exit
// exit:
//   %p0 = phi i8 [%e0, %pred1], [%x0, %pred2]
//   %p1 = phi i8 [%e1, %pred1], [%x1, %pred2]
//   %p2 = phi i8 [%e2, %pred1], [%x2, %pred2]
//   %p3 = phi i8 [%e3, %pred1], [%x3, %pred2]
//
// Pattern after:
// exit:
//   %pvec = phi <4 x i8> [%vec, %pred1], [%xvec, %pred2]
//   %p0 = extractelement <4 x i8> %pvec, i64 0
//   %p1 = extractelement <4 x i8> %pvec, i64 1
//   %p2 = extractelement <4 x i8> %pvec, i64 2
//   %p3 = extractelement <4 x i8> %pvec, i64 3
//
// This reduces register pressure by keeping values in vector registers.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/RevectorizeExtractPhi.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "revectorize-extract-phi"

STATISTIC(NumPhisRevectorized, "Number of phi groups revectorized");

namespace {

/// Represents a group of scalar phi nodes that can be combined into a vector.
struct PhiGroup {
  SmallVector<PHINode *, 4> Phis;        // Scalar phi nodes
  VectorType *VecTy;                     // The vector type
  SmallVector<Value *, 4> VecOperands;  // Vector operands for each incoming edge
};

/// Check if a value is an extractelement with constant index from a fixed vec.
static bool isConstantExtract(Value *V, Value *&Vec, uint64_t &Idx) {
  auto *EE = dyn_cast<ExtractElementInst>(V);
  if (!EE)
    return false;

  auto *IdxC = dyn_cast<ConstantInt>(EE->getIndexOperand());
  if (!IdxC)
    return false;

  auto *FVTy = dyn_cast<FixedVectorType>(EE->getVectorOperandType());
  if (!FVTy)
    return false;

  Vec = EE->getVectorOperand();
  Idx = IdxC->getZExtValue();
  return true;
}

/// Find groups of scalar phi nodes that can be revectorized.
static void findPhiGroups(BasicBlock &BB,
                          SmallVectorImpl<PhiGroup> &Groups) {
  // Group phi nodes by their incoming blocks and potential vector types
  DenseMap<std::pair<Type *, unsigned>, SmallVector<PHINode *, 8>> PhisByType;

  for (PHINode &PN : BB.phis()) {
    // Only handle small integer types that benefit from packing
    Type *Ty = PN.getType();
    if (!Ty->isIntegerTy(8) && !Ty->isIntegerTy(16))
      continue;

    unsigned NumIncoming = PN.getNumIncomingValues();
    auto Key = std::make_pair(Ty, NumIncoming);
    PhisByType[Key].push_back(&PN);
  }

  // For each type/incoming-count group, look for revectorization opportunities
  for (auto &[Key, Phis] : PhisByType) {
    (void)Key.first;  // ScalarTy unused
    unsigned NumIncoming = Key.second;

    if (Phis.size() < 4)
      continue;

    // Try to find groups of 4 that extract from the same vector(s)
    DenseMap<SmallVector<Value *, 4>, SmallVector<PHINode *, 8>> VecGroups;

    for (PHINode *PN : Phis) {
      SmallVector<Value *, 4> VecOperands;
      SmallVector<uint64_t, 4> Indices;
      bool AllExtracts = true;

      for (unsigned i = 0; i < NumIncoming && AllExtracts; ++i) {
        Value *InVal = PN->getIncomingValue(i);
        Value *Vec;
        uint64_t Idx;

        if (!isConstantExtract(InVal, Vec, Idx)) {
          AllExtracts = false;
          break;
        }

        VecOperands.push_back(Vec);
        if (i == 0) {
          // Record the index for the first incoming value
          Indices.push_back(Idx);
        } else if (Indices[0] != Idx) {
          // All incoming values should have the same index
          // (they're all element 0, or all element 1, etc.)
          AllExtracts = false;
        }
      }

      if (!AllExtracts)
        continue;

      // Add to group by vector operands
      VecGroups[VecOperands].push_back(PN);
    }

    // Check each vector group for complete sets of indices 0,1,2,3
    for (auto &[VecOps, GroupPhis] : VecGroups) {
      if (GroupPhis.size() != 4)
        continue;

      // Verify we have indices 0, 1, 2, 3
      std::array<PHINode *, 4> OrderedPhis = {};
      bool Valid = true;

      for (PHINode *PN : GroupPhis) {
        Value *FirstVal = PN->getIncomingValue(0);
        Value *Vec;
        uint64_t Idx;
        isConstantExtract(FirstVal, Vec, Idx);  // Already verified

        if (Idx >= 4 || OrderedPhis[Idx] != nullptr) {
          Valid = false;
          break;
        }
        OrderedPhis[Idx] = PN;
      }

      if (!Valid || !OrderedPhis[0] || !OrderedPhis[1] ||
          !OrderedPhis[2] || !OrderedPhis[3])
        continue;

      // Found a valid group
      PhiGroup Group;
      Group.Phis.assign(OrderedPhis.begin(), OrderedPhis.end());

      // Get vector type from first extract
      Value *FirstVec;
      uint64_t Idx;
      isConstantExtract(OrderedPhis[0]->getIncomingValue(0), FirstVec, Idx);
      Group.VecTy = cast<FixedVectorType>(
          cast<ExtractElementInst>(OrderedPhis[0]->getIncomingValue(0))
              ->getVectorOperandType());

      // Collect vector operands for each incoming edge
      for (unsigned i = 0; i < NumIncoming; ++i) {
        Value *Vec;
        uint64_t Idx;
        isConstantExtract(OrderedPhis[0]->getIncomingValue(i), Vec, Idx);
        Group.VecOperands.push_back(Vec);
      }

      Groups.push_back(std::move(Group));
    }
  }
}

/// Transform a phi group by creating a vector phi and extracting after.
static bool transformPhiGroup(PhiGroup &Group) {
  PHINode *FirstPhi = Group.Phis[0];
  BasicBlock *BB = FirstPhi->getParent();

  LLVM_DEBUG(dbgs() << "Revectorizing phi group in " << BB->getName() << "\n");

  // Create the new vector phi at the beginning of the block
  IRBuilder<> Builder(&*BB->getFirstInsertionPt());
  PHINode *VecPhi = Builder.CreatePHI(Group.VecTy, FirstPhi->getNumIncomingValues(),
                                      FirstPhi->getName() + ".vec");

  // Add incoming values to the vector phi
  for (unsigned i = 0; i < FirstPhi->getNumIncomingValues(); ++i) {
    VecPhi->addIncoming(Group.VecOperands[i], FirstPhi->getIncomingBlock(i));
  }

  // Find the first non-phi instruction to insert extracts
  BasicBlock::iterator InsertPt = BB->getFirstNonPHIIt();
  Builder.SetInsertPoint(&*InsertPt);

  // Create extractelement instructions and replace uses
  for (unsigned i = 0; i < 4; ++i) {
    Value *Extract = Builder.CreateExtractElement(VecPhi, i,
                                                  Group.Phis[i]->getName());
    Group.Phis[i]->replaceAllUsesWith(Extract);
  }

  // Erase the old scalar phi nodes
  for (PHINode *PN : Group.Phis) {
    PN->eraseFromParent();
  }

  ++NumPhisRevectorized;
  return true;
}

} // anonymous namespace

PreservedAnalyses RevectorizeExtractPhiPass::run(Function &F,
                                                  FunctionAnalysisManager &AM) {
  bool Changed = false;

  for (BasicBlock &BB : F) {
    SmallVector<PhiGroup, 16> Groups;
    findPhiGroups(BB, Groups);

    for (PhiGroup &G : Groups) {
      Changed |= transformPhiGroup(G);
    }
  }

  if (!Changed)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
