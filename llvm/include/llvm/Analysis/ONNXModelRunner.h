//===- ONNXModelRunner.h - ONNX Runtime model runner -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_ONNXMODELRUNNER_H
#define LLVM_ANALYSIS_ONNXMODELRUNNER_H

#include "llvm/Config/llvm-config.h"

#ifdef LLVM_HAVE_ONNXRUNTIME
#include "llvm/Analysis/MLModelRunner.h"
#include "llvm/Analysis/TensorSpec.h"
#include "llvm/IR/LLVMContext.h"
#include <memory>
#include <string>
#include <vector>

namespace llvm {

/// ONNXModelRunner - loads and evaluates an ONNX model using ONNX Runtime.
/// This provides an alternative to TFLite-based ModelUnderTrainingRunner,
/// allowing direct loading of PyTorch-exported ONNX models.
class ONNXModelRunner final : public MLModelRunner {
public:
  ONNXModelRunner(const ONNXModelRunner &) = delete;
  ONNXModelRunner &operator=(const ONNXModelRunner &) = delete;
  ~ONNXModelRunner() override;

  static std::unique_ptr<ONNXModelRunner>
  createAndEnsureValid(LLVMContext &Ctx, const std::string &ModelPath,
                       const std::vector<TensorSpec> &InputSpecs,
                       const std::string &OutputName);

  bool isValid() const { return Valid; }

  static bool classof(const MLModelRunner *R) {
    return R->getKind() == MLModelRunner::Kind::Development;
  }

private:
  ONNXModelRunner(LLVMContext &Ctx, const std::string &ModelPath,
                  const std::vector<TensorSpec> &InputSpecs,
                  const std::string &OutputName);

  void *evaluateUntyped() override;

  struct OrtState;
  std::unique_ptr<OrtState> State;
  bool Valid = false;
  int64_t ResultValue = 0;
  std::vector<TensorSpec> InputSpecs;
};

} // namespace llvm
#endif // LLVM_HAVE_ONNXRUNTIME
#endif // LLVM_ANALYSIS_ONNXMODELRUNNER_H
