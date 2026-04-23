//===- RevectorizeExtractPhi.h - Re-vectorize extracted scalar phis ------===//
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
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_REVECTORIZEEXTRACTPHI_H
#define LLVM_TRANSFORMS_SCALAR_REVECTORIZEEXTRACTPHI_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Function;

/// A pass that converts scalar phi nodes back to vector phi nodes
/// when the scalar values originated from extractelement instructions.
class RevectorizeExtractPhiPass
    : public PassInfoMixin<RevectorizeExtractPhiPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_REVECTORIZEEXTRACTPHI_H
