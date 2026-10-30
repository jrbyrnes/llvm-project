//===-- MLCoExecSchedStrategy.cpp - CoExec ML Scheduler Strategy ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Machine Learning guided co-execution scheduling strategy for AMDGPU.
/// Extends AMDGPUCoExecSchedStrategy with MLGO infrastructure.
///
//===----------------------------------------------------------------------===//

#include "MLCoExecSchedStrategy.h"
#include "AMDGPUBarrierLatency.h"
#include "AMDGPUIGroupLP.h"
#include "AMDGPUStaticSimulator.h"
#include "GCNSchedStrategy.h"
#include "SIMachineFunctionInfo.h"
#include "llvm/Analysis/NoInferenceModelRunner.h"
#include "llvm/Analysis/Utils/TrainingLogger.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/JSON.h"
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#ifdef LLVM_HAVE_TFLITE
#include "llvm/Analysis/ModelUnderTrainingRunner.h"
#endif

#ifdef LLVM_HAVE_ONNXRUNTIME
#include "llvm/Analysis/ONNXModelRunner.h"
#endif

#define DEBUG_TYPE "machine-scheduler"

using namespace llvm;

//===----------------------------------------------------------------------===//
// Command line options for MLGO
//===----------------------------------------------------------------------===//

static cl::opt<std::string> MLCoExecTrainingLog(
    "mlcoexec-training-log", cl::Hidden,
    cl::desc("Training log for the co-execution instruction scheduling model"));

static cl::opt<std::string> MLCoExecModel(
    "mlcoexec-model", cl::Hidden,
    cl::desc("The model being trained for co-execution instruction scheduling"));

static cl::opt<std::string> MLCoExecInferenceServer(
    "mlcoexec-inference-server", cl::Hidden,
    cl::desc("External inference server address (host:port) for online RL training"));

//===----------------------------------------------------------------------===//
// Feature definitions for ML model
// These will be expanded in future iterations to include CoExec-specific
// features like window state, hardware unit pressure, etc.
//===----------------------------------------------------------------------===//

// Decision tensor specification - the model outputs an index into candidates
#define SchedDecisionName "index_to_sched"
static const TensorSpec DecisionSpec =
    TensorSpec::createSpec<int64_t>(SchedDecisionName, {1});
static const TensorSpec Reward = TensorSpec::createSpec<float>("reward", {1});

// Feature declaration macros for X-macro pattern
#define _DECL_FEATURES(type, name, shape, _)                                   \
  TensorSpec::createSpec<type>(#name, shape),
#define _DECL_TRAIN_FEATURES(type, name, shape, _)                             \
  TensorSpec::createSpec<type>(std::string("action_") + #name, shape),

//===----------------------------------------------------------------------===//
// Feature table
// TODO: Extend with CoExec-specific features:
// - Current window state (active, slots remaining, filler demand)
// - Hardware unit pressure (per-unit remaining cycles, exposed counts)
// - Roofline analysis results
//===----------------------------------------------------------------------===//

static const int MaxNumInstCandidates = 128 * 2;
static const std::vector<int64_t> PerInstrFeatureShape{1, MaxNumInstCandidates};

// Basic features inherited from MLGCNSchedStrategy pattern
// These capture per-instruction scheduling state
#define SCHED_FEATURES_LIST(M)                                                 \
  M(int64_t, mask, PerInstrFeatureShape,                                       \
    "boolean values, 1 if the position is valid")                              \
  M(int64_t, is_top, PerInstrFeatureShape,                                     \
    "boolean values, 1 if the instruction is from Top Queue")                  \
  M(int64_t, is_bot, PerInstrFeatureShape,                                     \
    "boolean values, 1 if the instruction is from Bot Queue")                  \
  M(int64_t, pos, PerInstrFeatureShape,                                        \
    "position in the scheduling region (SU->NodeNum).")                        \
  M(int64_t, excess, PerInstrFeatureShape, "register pressure excess")         \
  M(int64_t, current_max, PerInstrFeatureShape, "current max pressure")        \
  M(int64_t, critical_max, PerInstrFeatureShape, "critical max pressure")      \
  M(int64_t, su_latency, PerInstrFeatureShape, "instruction latency")          \
  M(int64_t, su_height, PerInstrFeatureShape, "DAG height")                    \
  M(int64_t, su_depth, PerInstrFeatureShape, "DAG depth")                      \
  M(int64_t, su_succs_left, PerInstrFeatureShape, "remaining successors")      \
  M(int64_t, su_preds_left, PerInstrFeatureShape, "remaining predecessors")    \
  M(int64_t, su_succs, PerInstrFeatureShape, "total successors")               \
  M(int64_t, su_preds, PerInstrFeatureShape, "total predecessors")             \
  M(int64_t, sgpr_critical_limit, {1}, "SGPR critical pressure limit")         \
  M(int64_t, vgpr_critical_limit, {1}, "VGPR critical pressure limit")         \
  M(int64_t, sgpr_excess_limit, {1}, "SGPR excess pressure limit")             \
  M(int64_t, vgpr_excess_limit, {1}, "VGPR excess pressure limit")

// Named features index enum
enum SchedFeatureIDs {
#define _FEATURE_IDX_SIMPLE(_, name, __, ___) name
#define _FEATURE_IDX(A, B, C, D) _FEATURE_IDX_SIMPLE(A, B, C, D),
  SCHED_FEATURES_LIST(_FEATURE_IDX) FeatureCount,
#undef _FEATURE_IDX
#undef _FEATURE_IDX_SIMPLE
};

//===----------------------------------------------------------------------===//
// Helper functions
//===----------------------------------------------------------------------===//

template <typename T>
static size_t getTotalSize(const std::vector<int64_t> &Shape) {
  size_t Ret = sizeof(T);
  for (const auto V : Shape)
    Ret *= V;
  return Ret;
}

//===----------------------------------------------------------------------===//
// TCP Client for External Inference Server
//===----------------------------------------------------------------------===//

/// Simple TCP client for communicating with the Python inference server.
/// Uses JSON over TCP for simplicity and cross-platform compatibility.
class llvm::InferenceServerClient {
public:
  InferenceServerClient() = default;
  ~InferenceServerClient() {
    // Signal end of episode before disconnecting so server can preserve trajectory
    if (Connected)
      sendEndEpisode();
    disconnect();
  }

  /// Parse server address from "host:port" format.
  bool parseAddress(StringRef Address) {
    auto ColonPos = Address.find(':');
    if (ColonPos == StringRef::npos)
      return false;

    Host = Address.substr(0, ColonPos).str();
    StringRef PortStr = Address.substr(ColonPos + 1);

    if (PortStr.getAsInteger(10, Port))
      return false;

    return true;
  }

  /// Connect to the inference server.
  bool connect() {
    if (Connected)
      return true;

    SocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (SocketFD < 0) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: socket() failed\n");
      return false;
    }

    // Set non-blocking temporarily for connection timeout
    int flags = fcntl(SocketFD, F_GETFL, 0);
    fcntl(SocketFD, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(Port);

    // Resolve hostname
    struct hostent *he = gethostbyname(Host.c_str());
    if (!he) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: gethostbyname() failed for "
                        << Host << "\n");
      close(SocketFD);
      SocketFD = -1;
      return false;
    }
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Attempt connection
    int result = ::connect(SocketFD, (struct sockaddr *)&server_addr,
                           sizeof(server_addr));
    if (result < 0 && errno != EINPROGRESS) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: connect() failed\n");
      close(SocketFD);
      SocketFD = -1;
      return false;
    }

    // Wait for connection with timeout
    struct pollfd pfd;
    pfd.fd = SocketFD;
    pfd.events = POLLOUT;
    result = poll(&pfd, 1, 5000);  // 5 second timeout

    if (result <= 0) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: connection timeout\n");
      close(SocketFD);
      SocketFD = -1;
      return false;
    }

    // Check for connection errors
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(SocketFD, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ||
        error != 0) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: connection error\n");
      close(SocketFD);
      SocketFD = -1;
      return false;
    }

    // Set back to blocking mode
    fcntl(SocketFD, F_SETFL, flags);

    Connected = true;
    LLVM_DEBUG(dbgs() << "InferenceServerClient: connected to " << Host << ":"
                      << Port << "\n");
    return true;
  }

  /// Disconnect from the server.
  void disconnect() {
    if (SocketFD >= 0) {
      close(SocketFD);
      SocketFD = -1;
    }
    Connected = false;
  }

  /// Send feature data and receive action from server.
  /// Returns -1 on error.
  int64_t getAction(const json::Object &Features, float &LogProb) {
    if (!Connected && !connect())
      return -1;

    // Build request message
    json::Object Request;
    Request["type"] = "features";
    Request["data"] = json::Value(json::Object(Features));

    std::string RequestStr;
    raw_string_ostream OS(RequestStr);
    OS << json::Value(std::move(Request)) << "\n";
    OS.flush();

    // Send request
    ssize_t sent = send(SocketFD, RequestStr.data(), RequestStr.size(), 0);
    if (sent != static_cast<ssize_t>(RequestStr.size())) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: send() failed\n");
      disconnect();
      return -1;
    }

    // Receive response
    char buffer[4096];
    std::string ResponseStr;

    while (true) {
      struct pollfd pfd;
      pfd.fd = SocketFD;
      pfd.events = POLLIN;
      int ready = poll(&pfd, 1, 5000);  // 5 second timeout

      if (ready <= 0) {
        LLVM_DEBUG(dbgs() << "InferenceServerClient: recv() timeout\n");
        disconnect();
        return -1;
      }

      ssize_t received = recv(SocketFD, buffer, sizeof(buffer) - 1, 0);
      if (received <= 0) {
        LLVM_DEBUG(dbgs() << "InferenceServerClient: recv() failed\n");
        disconnect();
        return -1;
      }

      buffer[received] = '\0';
      ResponseStr += buffer;

      // Check for complete message (newline-terminated)
      if (ResponseStr.find('\n') != std::string::npos)
        break;
    }

    // Parse response
    Expected<json::Value> ParseResult = json::parse(ResponseStr);
    if (!ParseResult) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: JSON parse error\n");
      consumeError(ParseResult.takeError());
      return -1;
    }

    json::Object *Response = ParseResult->getAsObject();
    if (!Response) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: invalid response format\n");
      return -1;
    }

    // Extract action
    auto ActionVal = Response->getInteger("action");
    if (!ActionVal) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: missing action in response\n");
      return -1;
    }

    // Extract log_prob if present
    auto LogProbVal = Response->getNumber("log_prob");
    if (LogProbVal)
      LogProb = static_cast<float>(*LogProbVal);

    return *ActionVal;
  }

  /// Send reward signal to server.
  bool sendReward(float Reward, StringRef KernelName) {
    if (!Connected)
      return false;

    json::Object Request;
    Request["type"] = "reward";
    Request["value"] = Reward;
    Request["kernel"] = KernelName.str();

    std::string RequestStr;
    raw_string_ostream OS(RequestStr);
    OS << json::Value(std::move(Request)) << "\n";
    OS.flush();

    ssize_t sent = send(SocketFD, RequestStr.data(), RequestStr.size(), 0);
    if (sent != static_cast<ssize_t>(RequestStr.size())) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: sendReward() failed\n");
      return false;
    }

    // Wait for acknowledgment
    char buffer[256];
    struct pollfd pfd;
    pfd.fd = SocketFD;
    pfd.events = POLLIN;
    int ready = poll(&pfd, 1, 5000);

    if (ready > 0) {
      recv(SocketFD, buffer, sizeof(buffer), 0);
    }

    LLVM_DEBUG(dbgs() << "InferenceServerClient: sent reward " << Reward
                      << " for kernel " << KernelName << "\n");
    return true;
  }

  /// Signal end of episode to server (before disconnecting).
  /// This tells the server to move the current trajectory to pending,
  /// so it can be matched with the reward that comes on a separate connection.
  bool sendEndEpisode() {
    if (!Connected)
      return false;

    json::Object Request;
    Request["type"] = "end_episode";

    std::string RequestStr;
    raw_string_ostream OS(RequestStr);
    OS << json::Value(std::move(Request)) << "\n";
    OS.flush();

    ssize_t sent = send(SocketFD, RequestStr.data(), RequestStr.size(), 0);
    if (sent != static_cast<ssize_t>(RequestStr.size())) {
      LLVM_DEBUG(dbgs() << "InferenceServerClient: sendEndEpisode() failed\n");
      return false;
    }

    // Wait for acknowledgment
    char buffer[256];
    struct pollfd pfd;
    pfd.fd = SocketFD;
    pfd.events = POLLIN;
    int ready = poll(&pfd, 1, 5000);

    if (ready > 0) {
      recv(SocketFD, buffer, sizeof(buffer), 0);
    }

    LLVM_DEBUG(dbgs() << "InferenceServerClient: sent end_episode\n");
    return true;
  }

  bool isConnected() const { return Connected; }

private:
  std::string Host;
  int Port = 0;
  int SocketFD = -1;
  bool Connected = false;
};

static void resetRunnerInput(MLModelRunner &Runner) {
  LLVM_DEBUG(dbgs() << "=== resetRunnerInput (CoExec)\n");
#define _RESET(TYPE, NAME, SHAPE, __)                                          \
  std::memset(Runner.getTensorUntyped(SchedFeatureIDs::NAME), 0,               \
              getTotalSize<TYPE>(SHAPE));
  SCHED_FEATURES_LIST(_RESET)
#undef _RESET
}

//===----------------------------------------------------------------------===//
// MLCoExecSchedStrategy implementation
//===----------------------------------------------------------------------===//

MLCoExecSchedStrategy::SchedMode MLCoExecSchedStrategy::determineMode() {
  // If external inference server is specified, use Online mode
  if (!MLCoExecInferenceServer.empty())
    return SchedMode::Online;
  // If both model and log are empty, we're in default (no ML) mode
  if (MLCoExecModel.empty() && MLCoExecTrainingLog.empty())
    return SchedMode::Default;
  // If we have a model, we're in development/training mode
  // TODO: Add release mode support with AOT compiled model
  return SchedMode::Development;
}

MLCoExecSchedStrategy::MLCoExecSchedStrategy(const MachineSchedContext *C)
    : AMDGPUCoExecSchedStrategy(C), Mode(determineMode()) {
  // Initialize MLGO infrastructure after parent is fully constructed
  initializeMLGO(C);

  // Initialize external inference client if in Online mode
  if (Mode == SchedMode::Online) {
    InferenceClient = std::make_unique<InferenceServerClient>();
    if (!InferenceClient->parseAddress(MLCoExecInferenceServer)) {
      C->MF->getFunction().getContext().emitError(
          "MLCoExecSchedStrategy: invalid inference server address: " +
          MLCoExecInferenceServer);
      InferenceClient.reset();
    } else {
      LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: Online mode, server: "
                        << MLCoExecInferenceServer << "\n");
    }
  }
}

void MLCoExecSchedStrategy::initializeMLGO(const MachineSchedContext *C) {
  // In Default mode (no command line options), skip ML setup entirely
  if (Mode == SchedMode::Default) {
    Log = nullptr;
    Runner = nullptr;
    LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: Default mode, no ML "
                      << "infrastructure initialized\n");
    return;
  }

  // Build feature specifications (needed for both Online and Development modes)
  std::vector<TensorSpec> InputFeatures = {SCHED_FEATURES_LIST(_DECL_FEATURES)};
  LLVMContext &Ctx = C->MF->getFunction().getContext();

  // In Online mode, we need a NoInferenceModelRunner just to hold feature tensors
  // for serialization to JSON. The actual inference is done by the external server.
  if (Mode == SchedMode::Online) {
    Runner = std::make_unique<NoInferenceModelRunner>(Ctx, InputFeatures);
    Log = nullptr;
    LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: Online mode, "
                      << "NoInferenceModelRunner for feature tensors\n");
    return;
  }

  // Development mode: set up model runner and optional logging
  if (MLCoExecModel.empty() && MLCoExecTrainingLog.empty()) {
    Log = nullptr;
    Runner = nullptr;
    LLVM_DEBUG(
        dbgs() << "=== MLCoExecSchedStrategy: In development mode, logging or "
               << "a training model should be provided.\n");
    return;
  }
  std::vector<TensorSpec> TrainingInputFeatures = {
      SCHED_FEATURES_LIST(_DECL_TRAIN_FEATURES)
          TensorSpec::createSpec<float>("action_discount", {1}),
      TensorSpec::createSpec<int32_t>("action_step_type", {1}),
      TensorSpec::createSpec<float>("action_reward", {1})};

  // Create appropriate model runner based on configuration
  if (MLCoExecModel.empty()) {
    // No model provided - use NoInferenceModelRunner for logging only
    Runner = std::make_unique<NoInferenceModelRunner>(Ctx, InputFeatures);
  } else {
    bool ModelLoaded = false;
#ifdef LLVM_HAVE_ONNXRUNTIME
    // Try ONNX model if the path ends with .onnx
    if (StringRef(MLCoExecModel).ends_with(".onnx")) {
      Runner = ONNXModelRunner::createAndEnsureValid(
          Ctx, MLCoExecModel, InputFeatures, "logits");
      ModelLoaded = Runner != nullptr;
      LLVM_DEBUG(if (ModelLoaded)
        dbgs() << "MLCoExecSchedStrategy: loaded ONNX model from "
               << MLCoExecModel << "\n";
      else
        dbgs() << "MLCoExecSchedStrategy: FAILED to load ONNX model from "
               << MLCoExecModel << "\n";);
    }
#endif
#ifdef LLVM_HAVE_TFLITE
    if (!ModelLoaded) {
      Runner = ModelUnderTrainingRunner::createAndEnsureValid(
          Ctx, MLCoExecModel, SchedDecisionName, TrainingInputFeatures);
      ModelLoaded = Runner != nullptr;
    }
#endif
    if (!ModelLoaded) {
      Ctx.emitError(
          "MLCoExecSchedStrategy: could not load model. "
          "Build with LLVM_HAVE_ONNXRUNTIME=ON for .onnx models or "
          "LLVM_HAVE_TFLITE=ON for TFLite models.");
      return;
    }
  }

  if (!Runner) {
    Ctx.emitError("MLCoExecSchedStrategy: could not set up the model runner");
    return;
  }

  // Set up logging if training log path is provided
  if (MLCoExecTrainingLog.empty())
    return;

  std::error_code EC;
  auto OS = std::make_unique<raw_fd_ostream>(MLCoExecTrainingLog, EC);
  if (EC) {
    Ctx.emitError("MLCoExecSchedStrategy: " + EC.message() + ":" +
                  MLCoExecTrainingLog);
    return;
  }

  // Build logging feature specs
  std::vector<TensorSpec> LFS = InputFeatures;
#ifdef LLVM_HAVE_TFLITE
  if (auto *MUTR = dyn_cast<ModelUnderTrainingRunner>(Runner.get()))
    append_range(LFS, MUTR->extraOutputsForLoggingSpecs());
#endif
  LFS.push_back(DecisionSpec);

  Log = std::make_unique<Logger>(std::move(OS), LFS, Reward,
                                 /*IncludeReward*/ true);

  LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: MLGO initialized "
                    << "(Mode=Development)\n");
}

void MLCoExecSchedStrategy::switchContextForLog(StringRef Context) {
  if (Log == nullptr)
    return;

  LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: Switch context for ML data "
                    << "logging: " << Context << "\n");
  Log->switchContext(Context);
  Runner->switchContext(Context);
}

void MLCoExecSchedStrategy::logRewardIfNeeded() {
  // MF would be nullptr if `initialize` isn't called.
  // `initialize` is called from `schedule` when the number of instructions
  // is more than one.
  if (MF == nullptr)
    return;

  SIMachineFunctionInfo &MFI = *MF->getInfo<SIMachineFunctionInfo>();
  float reward = 0.0f;
  bool hasReward = false;

  // Use stall percentage from Static Simulator as the reward signal.
  // Reward = 1.0 - stall_percent, so higher is better (fewer stalls).
  // If the Static Simulator hasn't run yet (no report), fall back to occupancy.
  if (MFI.hasStaticSimReport()) {
    const AMDGPU::KernelPerfReport *Report = MFI.getStaticSimReport();
    // StallRatio is computed in finalize() as:
    // StallRatio = Scaled.StallCycles() / Scaled.TotalCycles
    reward = 1.0f - Report->StallRatio;
    hasReward = true;
    LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: reward from Static "
                      << "Simulator: " << reward
                      << " (StallRatio=" << Report->StallRatio << ")\n");
  } else {
    // Fallback to occupancy-based reward if no Static Simulator report
    reward = static_cast<float>(MFI.getOccupancy());
    hasReward = true;
    LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: occupancy-based "
                      << "reward (no Static Sim report): " << reward << "\n");
  }

  // Log reward to training log if available
  if (Log != nullptr && Log->hasObservationInProgress()) {
    Log->logReward<float>(reward);
  }

  // Send reward to external inference server if in Online mode
  if (Mode == SchedMode::Online && InferenceClient && hasReward) {
    StringRef KernelName = MF->getName();
    InferenceClient->sendReward(reward, KernelName);
    LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: sent reward " << reward
                      << " to inference server for kernel " << KernelName << "\n");
  }
}

//===----------------------------------------------------------------------===//
// Feature extraction methods
//===----------------------------------------------------------------------===//

void MLCoExecSchedStrategy::extractFeatures(SchedBoundary &Zone,
                                            const CandPolicy &ZonePolicy,
                                            const RegPressureTracker &RPTracker,
                                            bool IsBottomUp) {
  const SIRegisterInfo *SRI = static_cast<const SIRegisterInfo *>(TRI);
  ArrayRef<unsigned> Pressure = RPTracker.getRegSetPressureAtPos();
  unsigned SGPRPressure = 0;
  unsigned VGPRPressure = 0;
  unsigned AGPRPressure = 0;
  if (DAG->isTrackingPressure()) {
    SGPRPressure = Pressure[AMDGPU::RegisterPressureSets::SReg_32];
    VGPRPressure = Pressure[AMDGPU::RegisterPressureSets::VGPR_32];
    AGPRPressure = Pressure[AMDGPU::RegisterPressureSets::AGPR_32];
  }

  ReadyQueue &Q = Zone.Available;
  int64_t Pos = 0;
  LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy::extractFeatures ("
                    << (Zone.isTop() ? "Top" : "Bot") << ")\n");
  for (SUnit *SU : Q) {
    SchedCandidate Cand(ZonePolicy);
    initCandidate(Cand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                  VGPRPressure, AGPRPressure, IsBottomUp);
    extractCandidateFeatures(Cand, Pos++);
  }
}

void MLCoExecSchedStrategy::extractCandidateFeatures(SchedCandidate &TryCand,
                                                     int64_t Pos) const {
  // Set the features at the column 'Idx'.
  // if Candidate is from Bot, Idx = 2 * Pos
  // if Candidate is from Top, Idx = 2 * Pos + 1
  int64_t Idx = 2 * Pos + TryCand.AtTop;
  if (Idx >= MaxNumInstCandidates)
    return;

  LLVM_DEBUG(dbgs() << "[Position in Q: " << Pos << ", SU("
                    << TryCand.SU->NodeNum << ")]\n");
  LLVM_DEBUG(dbgs() << "    TryCand.AtTop: " << TryCand.AtTop << ", ");
  LLVM_DEBUG(dbgs() << "RPDelta.Excess: "
                    << TryCand.RPDelta.Excess.getUnitInc() << ", ");
  LLVM_DEBUG(dbgs() << "RPDelta.CurrentMax: "
                    << TryCand.RPDelta.CurrentMax.getUnitInc() << ", ");
  LLVM_DEBUG(dbgs() << "RPDelta.CriticalMax: "
                    << TryCand.RPDelta.CriticalMax.getUnitInc() << "\n");
  LLVM_DEBUG(dbgs() << "    SU->Latency: " << TryCand.SU->Latency << ", ");
  LLVM_DEBUG(dbgs() << "SU->getHeight: " << TryCand.SU->getHeight() << ", ");
  LLVM_DEBUG(dbgs() << "SU->getDepth: " << TryCand.SU->getDepth() << "\n");
  LLVM_DEBUG(dbgs() << "    SU->NumSuccsLeft: " << TryCand.SU->NumSuccsLeft
                    << ", ");
  LLVM_DEBUG(dbgs() << "SU->NumPredsLeft: " << TryCand.SU->NumPredsLeft << ", ");
  LLVM_DEBUG(dbgs() << "SU->NumSuccs: " << TryCand.SU->NumSuccs << ", ");
  LLVM_DEBUG(dbgs() << "SU->NumPreds: " << TryCand.SU->NumPreds << "\n");

#define SET(ID, TYPE, VAL)                                                     \
  do {                                                                         \
    Runner->getTensor<TYPE>(SchedFeatureIDs::ID)[Idx] =                        \
        static_cast<TYPE>(VAL);                                                \
  } while (false)
  SET(mask, int64_t, 1);
  SET(is_top, int64_t, TryCand.AtTop);
  SET(is_bot, int64_t, !TryCand.AtTop);
  SET(pos, int64_t, TryCand.SU->NodeNum);
  SET(excess, int64_t, TryCand.RPDelta.Excess.getUnitInc());
  SET(current_max, int64_t, TryCand.RPDelta.CurrentMax.getUnitInc());
  SET(critical_max, int64_t, TryCand.RPDelta.CriticalMax.getUnitInc());
  SET(su_latency, int64_t, TryCand.SU->Latency);
  SET(su_height, int64_t, TryCand.SU->getHeight());
  SET(su_depth, int64_t, TryCand.SU->getDepth());
  SET(su_succs_left, int64_t, TryCand.SU->NumSuccsLeft);
  SET(su_preds_left, int64_t, TryCand.SU->NumPredsLeft);
  SET(su_succs, int64_t, TryCand.SU->NumSuccs);
  SET(su_preds, int64_t, TryCand.SU->NumPreds);
#undef SET
}

void MLCoExecSchedStrategy::extractGlobalFeatures() {
  LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy::extractGlobalFeatures\n");
  LLVM_DEBUG(dbgs() << "    SGPRCriticalLimit: " << SGPRCriticalLimit
                    << ", VGPRCriticalLimit: " << VGPRCriticalLimit << "\n");
  LLVM_DEBUG(dbgs() << "    SGPRExcessLimit: " << SGPRExcessLimit
                    << ", VGPRExcessLimit: " << VGPRExcessLimit << "\n");

#define SET(ID, TYPE, VAL)                                                     \
  do {                                                                         \
    *Runner->getTensor<TYPE>(SchedFeatureIDs::ID) = static_cast<TYPE>(VAL);    \
  } while (false)
  SET(sgpr_critical_limit, int64_t, SGPRCriticalLimit);
  SET(vgpr_critical_limit, int64_t, VGPRCriticalLimit);
  SET(sgpr_excess_limit, int64_t, SGPRExcessLimit);
  SET(vgpr_excess_limit, int64_t, VGPRExcessLimit);
#undef SET
}

//===----------------------------------------------------------------------===//
// logMLFeatures implementation
//===----------------------------------------------------------------------===//

void MLCoExecSchedStrategy::logMLFeatures(int64_t SchedIndex) {
  LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy::logMLFeatures: schedule "
                    << "index: " << SchedIndex << "\n");

  if (SchedIndex >= MaxNumInstCandidates) {
    resetRunnerInput(*Runner);
    return;
  }

  // Log intermediate reward (0.0) if an observation is in progress
  if (Log->hasObservationInProgress())
    Log->logReward<float>(0.0);

  // Start a new observation and log all feature tensors
  Log->startObservation();
  size_t CurrentFeature = 0;
  for (; CurrentFeature < SchedFeatureIDs::FeatureCount; ++CurrentFeature) {
    Log->logTensorValue(
        CurrentFeature,
        reinterpret_cast<const char *>(getRunner().getTensorUntyped(CurrentFeature)));
  }

  // Log extra outputs from ModelUnderTrainingRunner if available
#ifdef LLVM_HAVE_TFLITE
  if (auto *MUTR = dyn_cast<ModelUnderTrainingRunner>(&getRunner()))
    for (size_t I = 0; I < MUTR->extraOutputsForLoggingSpecs().size();
         ++I, ++CurrentFeature)
      Log->logTensorValue(
          CurrentFeature,
          reinterpret_cast<const char *>(MUTR->getUntypedExtraOutputValue(I)));
#endif

  // Log the decision index
  Log->logTensorValue(CurrentFeature,
                      reinterpret_cast<const char *>(&SchedIndex));
  Log->endObservation();
}

//===----------------------------------------------------------------------===//
// serializeFeaturesToJSON implementation
//===----------------------------------------------------------------------===//

json::Object MLCoExecSchedStrategy::serializeFeaturesToJSON() const {
  json::Object Features;
  json::Object PerInstr;
  json::Object Global;

  // Serialize per-instruction features
#define SERIALIZE_PER_INSTR(TYPE, NAME, SHAPE, DESC) \
  { \
    json::Array Values; \
    const TYPE *Data = Runner->getTensor<TYPE>(SchedFeatureIDs::NAME); \
    for (int i = 0; i < MaxNumInstCandidates; ++i) { \
      Values.push_back(static_cast<int64_t>(Data[i])); \
    } \
    PerInstr[#NAME] = std::move(Values); \
  }

  // Per-instruction features
  SERIALIZE_PER_INSTR(int64_t, mask, PerInstrFeatureShape, "mask")
  SERIALIZE_PER_INSTR(int64_t, is_top, PerInstrFeatureShape, "is_top")
  SERIALIZE_PER_INSTR(int64_t, is_bot, PerInstrFeatureShape, "is_bot")
  SERIALIZE_PER_INSTR(int64_t, pos, PerInstrFeatureShape, "pos")
  SERIALIZE_PER_INSTR(int64_t, excess, PerInstrFeatureShape, "excess")
  SERIALIZE_PER_INSTR(int64_t, current_max, PerInstrFeatureShape, "current_max")
  SERIALIZE_PER_INSTR(int64_t, critical_max, PerInstrFeatureShape, "critical_max")
  SERIALIZE_PER_INSTR(int64_t, su_latency, PerInstrFeatureShape, "su_latency")
  SERIALIZE_PER_INSTR(int64_t, su_height, PerInstrFeatureShape, "su_height")
  SERIALIZE_PER_INSTR(int64_t, su_depth, PerInstrFeatureShape, "su_depth")
  SERIALIZE_PER_INSTR(int64_t, su_succs_left, PerInstrFeatureShape, "su_succs_left")
  SERIALIZE_PER_INSTR(int64_t, su_preds_left, PerInstrFeatureShape, "su_preds_left")
  SERIALIZE_PER_INSTR(int64_t, su_succs, PerInstrFeatureShape, "su_succs")
  SERIALIZE_PER_INSTR(int64_t, su_preds, PerInstrFeatureShape, "su_preds")

#undef SERIALIZE_PER_INSTR

  // Global features
  Global["sgpr_critical_limit"] = *Runner->getTensor<int64_t>(SchedFeatureIDs::sgpr_critical_limit);
  Global["vgpr_critical_limit"] = *Runner->getTensor<int64_t>(SchedFeatureIDs::vgpr_critical_limit);
  Global["sgpr_excess_limit"] = *Runner->getTensor<int64_t>(SchedFeatureIDs::sgpr_excess_limit);
  Global["vgpr_excess_limit"] = *Runner->getTensor<int64_t>(SchedFeatureIDs::vgpr_excess_limit);

  Features["per_instr"] = std::move(PerInstr);
  Features["global"] = std::move(Global);

  return Features;
}

//===----------------------------------------------------------------------===//
// pickNodeFromServer implementation
//===----------------------------------------------------------------------===//

SUnit *MLCoExecSchedStrategy::pickNodeFromServer(bool &IsTopNode) {
  if (!InferenceClient)
    return nullptr;

  // Serialize current features to JSON
  json::Object Features = serializeFeaturesToJSON();

  // Send to server and get action
  float LogProb = 0.0f;
  int64_t Ret = InferenceClient->getAction(Features, LogProb);

  LLVM_DEBUG(dbgs() << "=*= MLCoExecSchedStrategy::pickNodeFromServer: Ret = "
                    << Ret << ", LogProb = " << LogProb << "\n");

  if (Ret < 0) {
    LLVM_DEBUG(dbgs() << "Server returned negative, using heuristics instead\n");
    return nullptr;
  }

  // Determine queue based on Ret parity
  // Encoding: SchedIndex = 2 * Pos + IsTopNode (Top=1, Bot=0)
  bool pickFromTop = (Ret % 2) != 0;
  IsTopNode = pickFromTop;
  if (pickFromTop) {
    LLVM_DEBUG(dbgs() << "    Picking from TopQ by server: ");
    --Ret;
  } else {
    LLVM_DEBUG(dbgs() << "    Picking from BotQ by server: ");
  }

  ReadyQueue &Q = pickFromTop ? Top.Available : Bot.Available;
  size_t index = static_cast<size_t>(Ret / 2);

  if (index >= Q.size()) {
    LLVM_DEBUG(dbgs() << "pickNodeByModel: index=" << index
                      << " >= QSize=" << Q.size() << "\n");
    return nullptr;
  }

  return Q.elements()[index];
}

//===----------------------------------------------------------------------===//
// pickNodeByModel implementation
//===----------------------------------------------------------------------===//

SUnit *MLCoExecSchedStrategy::pickNodeByModel(bool &IsTopNode) {
  int64_t Ret = Runner->evaluate<int64_t>();

  if (Ret < 0) {
    LLVM_DEBUG(dbgs() << "pickNodeByModel: Ret=" << Ret << " (negative)\n");
    return nullptr;
  }

  // Determine queue based on Ret parity
  // Encoding: SchedIndex = 2 * Pos + IsTopNode (Top=1, Bot=0)
  bool pickFromTop = (Ret % 2) != 0;
  IsTopNode = pickFromTop;
  if (pickFromTop) {
    LLVM_DEBUG(dbgs() << "    Picking from TopQ by model: ");
    --Ret;
  } else {
    LLVM_DEBUG(dbgs() << "    Picking from BotQ by model: ");
  }

  ReadyQueue &Q = pickFromTop ? Top.Available : Bot.Available;
  size_t index = static_cast<size_t>(Ret / 2);

  if (index >= Q.size()) {
    LLVM_DEBUG(dbgs() << "Index is out of bound\n");
    return nullptr;
  }

  LLVM_DEBUG(dbgs() << "SU(" << Q.elements()[index]->NodeNum << ")\n");
  return Q.elements()[index];
}

//===----------------------------------------------------------------------===//
// pickNode implementation
//===----------------------------------------------------------------------===//

SUnit *MLCoExecSchedStrategy::pickNode(bool &IsTopNode) {
  LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy::pickNode\n");

  // Check for empty DAG
  if (DAG->top() == DAG->bottom()) {
    LLVM_DEBUG(dbgs() << "DAG: top == bottom\n");
    assert(Top.Available.empty() && Top.Pending.empty() &&
           Bot.Available.empty() && Bot.Pending.empty() && "ReadyQ garbage");
    return nullptr;
  }

  // Extract features if we have a runner (for both inference and logging)
  // In Online mode, Runner is always initialized (as NoInferenceModelRunner)
  if (Runner != nullptr) {
    // Reset all feature tensors to zero
    resetRunnerInput(*Runner);

    // Create policies for feature extraction (mirrors parent's pickNode)
    CandPolicy BotPolicy;
    setPolicy(BotPolicy, /*IsPostRA=*/false, Bot, &Top);
    CandPolicy TopPolicy;
    setPolicy(TopPolicy, /*IsPostRA=*/false, Top, &Bot);

    extractFeatures(Top, TopPolicy, DAG->getTopRPTracker(),
                    /*IsBottomUp=*/false);
    extractFeatures(Bot, BotPolicy, DAG->getBotRPTracker(),
                    /*IsBottomUp=*/true);
    extractGlobalFeatures();
  }

  SUnit *SU = nullptr;

  LLVM_DEBUG(dbgs() << "MLCoExecSchedStrategy: Mode="
                    << (Mode == SchedMode::Online ? "Online" :
                        Mode == SchedMode::Release ? "Release" :
                        Mode == SchedMode::Development ? "Development" : "Default")
                    << "\n");

  // Try Online mode (external inference server) first
  if (Mode == SchedMode::Online && InferenceClient) {
    SU = pickNodeFromServer(IsTopNode);
  }
  // Try ML model inference in Release mode or when model is loaded
  else if (Mode == SchedMode::Release) {
    // Release mode: always use model for inference
    SU = pickNodeByModel(IsTopNode);
  }
#ifdef LLVM_HAVE_TFLITE
  else if (Runner && isa<ModelUnderTrainingRunner>(Runner.get())) {
    // Development mode with TFLite model loaded: use model for inference
    SU = pickNodeByModel(IsTopNode);
  }
#endif
#ifdef LLVM_HAVE_ONNXRUNTIME
  else if (Runner && isa<ONNXModelRunner>(Runner.get())) {
    SU = pickNodeByModel(IsTopNode);
  }
#endif

  // If model/server returned a valid node, remove it from queues and return
  if (SU) {
    LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: ML/Server picked SU("
                      << SU->NodeNum << ")\n");

    // Log features and decision if logging is enabled
    if (Log != nullptr) {
      ReadyQueue &Q = IsTopNode ? Top.Available : Bot.Available;
      auto It = llvm::find(Q, SU);
      if (It != Q.end()) {
        int64_t Pos = It - Q.begin();
        int64_t SchedIndex = 2 * Pos + IsTopNode;
        logMLFeatures(SchedIndex);
      }
    }

    // Remove from ready queues
    if (SU->isTopReady())
      Top.removeReady(SU);
    if (SU->isBottomReady())
      Bot.removeReady(SU);

    LLVM_DEBUG(dbgs() << "Scheduling SU(" << SU->NodeNum << ") "
                      << *SU->getInstr());
    return SU;
  }

  // Fall back to parent CoExec strategy if:
  // - Default mode (no ML)
  // - Model/server returned invalid index
  // - NoInferenceModelRunner (logging only, no model decisions)
  LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: Falling back to parent "
                    << "heuristics\n");

  // Save queue state BEFORE calling parent, since parent removes the picked SU
  // We need to find the position of the picked node for logging
  SmallVector<SUnit *, 16> TopQueueCopy, BotQueueCopy;
  if (Log != nullptr) {
    for (SUnit *S : Top.Available)
      TopQueueCopy.push_back(S);
    for (SUnit *S : Bot.Available)
      BotQueueCopy.push_back(S);
  }

  SU = AMDGPUCoExecSchedStrategy::pickNode(IsTopNode);

  // Log features and decision if logging is enabled and we picked a node
  if (Log != nullptr && SU != nullptr) {
    // Find the position of the picked SU in the saved queue copy
    // SchedIndex = 2 * Pos + IsTopNode (matching extractCandidateFeatures)
    auto &QCopy = IsTopNode ? TopQueueCopy : BotQueueCopy;
    auto It = llvm::find(QCopy, SU);
    if (It != QCopy.end()) {
      int64_t Pos = It - QCopy.begin();
      int64_t SchedIndex = 2 * Pos + IsTopNode;
      LLVM_DEBUG(dbgs() << "=== MLCoExecSchedStrategy: Picked SU("
                        << SU->NodeNum << ") at Pos=" << Pos
                        << ", IsTopNode=" << IsTopNode
                        << ", SchedIndex=" << SchedIndex << "\n");
      logMLFeatures(SchedIndex);
    }
  }

  return SU;
}

//===----------------------------------------------------------------------===//
// Factory function
//===----------------------------------------------------------------------===//

ScheduleDAGInstrs *
llvm::createGCNMLCoExecMachineScheduler(MachineSchedContext *C) {
  LLVM_DEBUG(dbgs() << "AMDGPU MLCoExec preRA scheduler selected for "
                    << C->MF->getName() << '\n');

  ScheduleDAGMILive *DAG = new GCNScheduleDAGMILive(
      C, std::make_unique<MLCoExecSchedStrategy>(C));

  // Add the same mutations as the regular CoExec scheduler
  DAG->addMutation(createIGroupLPDAGMutation(AMDGPU::SchedulingPhase::Initial));
  DAG->addMutation(createAMDGPUBarrierLatencyDAGMutation(C->MF));
  return DAG;
}
