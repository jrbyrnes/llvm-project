//===- ONNXModelRunner.cpp - ONNX Runtime model runner --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Config/llvm-config.h"
#ifdef LLVM_HAVE_ONNXRUNTIME

#include "llvm/Analysis/ONNXModelRunner.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <onnxruntime_c_api.h>
#include <cassert>
#include <cstring>
#include <numeric>

#define DEBUG_TYPE "onnx-model-runner"

using namespace llvm;

// ONNX Runtime state held behind a pimpl to avoid leaking the ORT headers.
struct ONNXModelRunner::OrtState {
  const OrtApi *Api = nullptr;
  OrtEnv *Env = nullptr;
  OrtSession *Session = nullptr;
  OrtSessionOptions *SessionOpts = nullptr;
  OrtMemoryInfo *MemInfo = nullptr;

  // Pre-allocated buffers for model I/O.
  // per_instr: [1, MaxCandidates, NumPerInstrFeatures] float32
  // global:    [1, NumGlobalFeatures] float32
  std::vector<float> PerInstrBuf;
  std::vector<float> GlobalBuf;
  std::vector<float> OutputBuf;

  size_t NumPerInstrFeatures = 0;
  size_t NumGlobalFeatures = 0;
  size_t MaxCandidates = 0;
  size_t NumOutputs = 0;

  // Owned strings for ONNX input/output names.
  std::string OutputNameStr;
  // Pointers into owned strings (must stay alive for ORT API calls).
  std::vector<const char *> InputNames;
  std::vector<const char *> OutputNames;

  ~OrtState() {
    if (Api) {
      if (Session)
        Api->ReleaseSession(Session);
      if (SessionOpts)
        Api->ReleaseSessionOptions(SessionOpts);
      if (MemInfo)
        Api->ReleaseMemoryInfo(MemInfo);
      if (Env)
        Api->ReleaseEnv(Env);
    }
  }
};

static bool checkStatus(const OrtApi *Api, OrtStatus *Status) {
  if (Status != nullptr) {
    LLVM_DEBUG(dbgs() << "ONNX Runtime error: " << Api->GetErrorMessage(Status)
                      << "\n");
    Api->ReleaseStatus(Status);
    return false;
  }
  return true;
}

ONNXModelRunner::ONNXModelRunner(LLVMContext &Ctx,
                                 const std::string &ModelPath,
                                 const std::vector<TensorSpec> &Inputs,
                                 const std::string &OutputName)
    : MLModelRunner(Ctx, MLModelRunner::Kind::Development, Inputs.size()),
      State(std::make_unique<OrtState>()), InputSpecs(Inputs) {

  // Set up input buffers for each TensorSpec so the rest of LLVM can write
  // feature values via getTensor<T>(FeatureID).
  for (size_t I = 0; I < Inputs.size(); ++I)
    setUpBufferForTensor(I, Inputs[I], nullptr);

  // Classify features by shape: per-instruction ([1, N]) vs global ([1]).
  for (const auto &Spec : Inputs) {
    const auto &Shape = Spec.shape();
    if (Shape.size() == 2 && Shape[1] > 1) {
      State->MaxCandidates = static_cast<size_t>(Shape[1]);
      State->NumPerInstrFeatures++;
    } else {
      State->NumGlobalFeatures++;
    }
  }

  LLVM_DEBUG(dbgs() << "ONNXModelRunner: " << State->NumPerInstrFeatures
                    << " per-instr features, " << State->NumGlobalFeatures
                    << " global features, MaxCandidates="
                    << State->MaxCandidates << "\n");

  State->NumOutputs = State->MaxCandidates * 2;
  State->PerInstrBuf.resize(State->MaxCandidates *
                            State->NumPerInstrFeatures);
  State->GlobalBuf.resize(State->NumGlobalFeatures);
  State->OutputBuf.resize(State->NumOutputs);

  // Initialise ONNX Runtime.
  State->Api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
  if (!State->Api) {
    LLVM_DEBUG(dbgs() << "ONNXModelRunner: failed to get ORT API\n");
    return;
  }

  if (!checkStatus(State->Api,
                   State->Api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                         "llvm-mlgo", &State->Env)))
    return;

  if (!checkStatus(State->Api,
                   State->Api->CreateSessionOptions(&State->SessionOpts)))
    return;

  State->Api->SetIntraOpNumThreads(State->SessionOpts, 1);
  State->Api->SetSessionGraphOptimizationLevel(State->SessionOpts,
                                               ORT_ENABLE_BASIC);

  if (!checkStatus(State->Api,
                   State->Api->CreateSession(State->Env, ModelPath.c_str(),
                                             State->SessionOpts,
                                             &State->Session)))
    return;

  if (!checkStatus(
          State->Api,
          State->Api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
                                          &State->MemInfo)))
    return;

  State->InputNames = {"per_instr", "global"};
  State->OutputNameStr = OutputName;
  State->OutputNames = {State->OutputNameStr.c_str()};

  Valid = true;
  LLVM_DEBUG(dbgs() << "ONNXModelRunner: loaded " << ModelPath << "\n");
}

ONNXModelRunner::~ONNXModelRunner() = default;

std::unique_ptr<ONNXModelRunner>
ONNXModelRunner::createAndEnsureValid(LLVMContext &Ctx,
                                      const std::string &ModelPath,
                                      const std::vector<TensorSpec> &InputSpecs,
                                      const std::string &OutputName) {
  auto Runner = std::unique_ptr<ONNXModelRunner>(
      new ONNXModelRunner(Ctx, ModelPath, InputSpecs, OutputName));
  if (!Runner->isValid())
    return nullptr;
  return Runner;
}

void *ONNXModelRunner::evaluateUntyped() {
  if (!Valid) {
    ResultValue = -1;
    return &ResultValue;
  }

  // Pack LLVM feature buffers into the two ONNX input tensors.
  // Per-instruction features: each is a [1, MaxCandidates] int64_t buffer.
  // We need to produce a [1, MaxCandidates, NumPerInstrFeatures] float32 tensor
  // with features interleaved per candidate.
  size_t PerInstrIdx = 0;
  size_t GlobalIdx = 0;
  const int64_t *MaskData = nullptr;

  for (size_t I = 0; I < InputSpecs.size(); ++I) {
    const auto &Shape = InputSpecs[I].shape();
    if (Shape.size() == 2 && Shape[1] > 1) {
      // Per-instruction feature — copy into column-major layout.
      const int64_t *Src =
          reinterpret_cast<const int64_t *>(getTensorUntyped(I));
      if (PerInstrIdx == 0)
        MaskData = Src;
      for (size_t C = 0; C < State->MaxCandidates; ++C)
        State->PerInstrBuf[C * State->NumPerInstrFeatures + PerInstrIdx] =
            static_cast<float>(Src[C]);
      PerInstrIdx++;
    } else {
      // Global feature — single scalar.
      const int64_t *Src =
          reinterpret_cast<const int64_t *>(getTensorUntyped(I));
      State->GlobalBuf[GlobalIdx++] = static_cast<float>(Src[0]);
    }
  }

  // Create ORT tensors wrapping our buffers (no copy).
  int64_t PerInstrDims[3] = {1, static_cast<int64_t>(State->MaxCandidates),
                             static_cast<int64_t>(State->NumPerInstrFeatures)};
  int64_t GlobalDims[2] = {1, static_cast<int64_t>(State->NumGlobalFeatures)};

  OrtValue *InputTensors[2] = {nullptr, nullptr};

  if (!checkStatus(
          State->Api,
          State->Api->CreateTensorWithDataAsOrtValue(
              State->MemInfo, State->PerInstrBuf.data(),
              State->PerInstrBuf.size() * sizeof(float), PerInstrDims, 3,
              ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &InputTensors[0])))
    goto cleanup;

  if (!checkStatus(
          State->Api,
          State->Api->CreateTensorWithDataAsOrtValue(
              State->MemInfo, State->GlobalBuf.data(),
              State->GlobalBuf.size() * sizeof(float), GlobalDims, 2,
              ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &InputTensors[1])))
    goto cleanup;

  {
    OrtValue *OutputTensor = nullptr;
    if (!checkStatus(State->Api,
                     State->Api->Run(State->Session, nullptr,
                                     State->InputNames.data(), InputTensors, 2,
                                     State->OutputNames.data(), 1,
                                     &OutputTensor))) {
      ResultValue = -1;
      goto cleanup;
    }

    // Read output logits.
    float *OutputData = nullptr;
    if (!checkStatus(
            State->Api,
            State->Api->GetTensorMutableData(OutputTensor,
                                             reinterpret_cast<void **>(
                                                 &OutputData)))) {
      State->Api->ReleaseValue(OutputTensor);
      ResultValue = -1;
      goto cleanup;
    }

    // Count valid mask entries for diagnostics.
    size_t MaskCount = 0;
    if (MaskData)
      for (size_t I = 0; I < State->MaxCandidates; ++I)
        if (MaskData[I] != 0)
          ++MaskCount;

    // Masked argmax: pick the highest-scoring valid action.
    float BestScore = -1e30f;
    int64_t BestIdx = -1;
    for (size_t I = 0; I < State->NumOutputs; ++I) {
      // Decode: SchedIndex = 2 * Pos + IsTopNode
      size_t Pos = I / 2;
      if (Pos < State->MaxCandidates && MaskData && MaskData[Pos] == 0)
        continue;
      if (OutputData[I] > BestScore) {
        BestScore = OutputData[I];
        BestIdx = static_cast<int64_t>(I);
      }
    }
    ResultValue = BestIdx;

    LLVM_DEBUG(dbgs() << "ONNXModelRunner: mask=" << MaskCount
                      << " valid, argmax=" << ResultValue
                      << " score=" << BestScore << "\n");

    LLVM_DEBUG(dbgs() << "ONNXModelRunner: argmax=" << ResultValue
                      << " score=" << BestScore << "\n");

    State->Api->ReleaseValue(OutputTensor);
  }

cleanup:
  if (InputTensors[0])
    State->Api->ReleaseValue(InputTensors[0]);
  if (InputTensors[1])
    State->Api->ReleaseValue(InputTensors[1]);

  return &ResultValue;
}

#endif // LLVM_HAVE_ONNXRUNTIME
