#include "frame_driver.h"

#include "async_disc_owner.h"
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "native_ownership.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>
#include <span>

namespace ctr {
namespace {

struct FrameCompleted final {};

struct OverrideBinding {
  uint32_t address;
  CtrRuntime::RecompiledOverride function;
};

class ScopedFrameOverrides final {
public:
  ScopedFrameOverrides(CtrRuntime &runtime, std::span<const OverrideBinding> bindings)
      : runtime_(runtime), bindings_(bindings) {
    for (const OverrideBinding &binding : bindings_) {
      runtime_.setRecompiledOverride(binding.address, binding.function);
    }
  }

  ~ScopedFrameOverrides() {
    for (const OverrideBinding &binding : bindings_) {
      runtime_.setRecompiledOverride(binding.address, nullptr);
    }
  }

  ScopedFrameOverrides(const ScopedFrameOverrides &) = delete;
  ScopedFrameOverrides &operator=(const ScopedFrameOverrides &) = delete;

private:
  CtrRuntime &runtime_;
  std::span<const OverrideBinding> bindings_;
};

class ScopedActiveDriver final {
public:
  explicit ScopedActiveDriver(CtrFrameDriver *&slot, CtrFrameDriver &driver) : slot_(slot) {
    if (slot_) {
      lucent::error("ctr-frame", "nested CTR frame drivers are not supported");
      std::abort();
    }
    slot_ = &driver;
  }

  ~ScopedActiveDriver() {
    slot_ = nullptr;
  }

  ScopedActiveDriver(const ScopedActiveDriver &) = delete;
  ScopedActiveDriver &operator=(const ScopedActiveDriver &) = delete;

private:
  CtrFrameDriver *&slot_;
};

[[noreturn]] void wrongReturn(const char *owner, uint32_t expected, uint32_t actual) {
  lucent::error(
      "ctr-frame", "{} reached from 0x{:08X}; expected exact retail return 0x{:08X}", owner, actual, expected);
  std::abort();
}

} // namespace

CtrFrameDriver *CtrFrameDriver::active_ = nullptr;

CtrFrameDriver::CtrFrameDriver(CtrRuntime &runtime) : runtime_(runtime) {}

void CtrFrameDriver::stepFrame(Core &core, uint32_t frame) {
  if (!core.game) {
    lucent::error("ctr-frame", "frame {} has no bound Game", frame);
    std::abort();
  }
  if (frame != completedFrames_) {
    lucent::error(
        "ctr-frame", "non-sequential frame {} requested after {} completed frame(s)", frame, completedFrames_);
    std::abort();
  }

  const std::array<OverrideBinding, 10> bindings{{
      {native::kStartupGpuInit, skipFirstStartupVSync},
      {native::kStartupDisplayInit, skipSecondStartupVSync},
      {native::kBootResourceWait, waitForBootResourceWithoutVSync},
      {native::kBootResourcePump, pumpBootResourceWithoutBusyWait},
      {native::kStartupAudioService, serviceStartupAudioWithoutBusyWait},
      {native::kStartupAudioLoop, continueStartupAudioLoop},
      {native::kShutdownDisplay, skipShutdownVSync},
      {native::kVblankCallbackInstall, observeVblankCallback},
      {native::kFrameTiming, finishFrameWithoutDebugVSync},
      {native::kProjectionProducer, publishProjection},
  }};
  ScopedFrameOverrides overrides(runtime_, std::span{bindings});
  ScopedActiveDriver active(active_, *this);
  const int attributionDepth = core.idiag.otattr_depth;

  try {
    core.game->timing.logicFrame = frame;
    core.rsub.otAttr.beginLogicFrame(frame);
    core.game->timing.frameTick();
    frameCallbacks_.deliverField(core, runtime_);
    core.game->pad.serviceFrame();
    // XA playback is pull-driven by the SPU mixer. State-zero waits for the startup clip task at
    // 0x8008D708 to finish, so every host-owned field must advance audio even before the first
    // visible presentation; otherwise the decoded ring fills and retail execution cannot leave.
    core.game->spu_audio.frame();
    // A libcd read completion is an interrupt in retail, so it is delivered at this per-field seam
    // rather than inside the CdRead leaf: the loader stores its allocated buffer into the queue
    // entry only after the read call returns, and the completion chain reads that same field.
    discReadOwner().deliverPending(core, runtime_);
    dmaCallbacks_.serviceSpu(core, runtime_);
    if (dmaCallbacks_.hasPendingSpu()) {
      throw FrameCompleted{};
    }

    if (frameSuffixPending_) {
      resumeFrameSuffix(core);
    } else if (startupAudioWaitResume_ != 0) {
      resumeStartupAudioWait(core);
    } else if (bootResourcePumpPhase_ != BootResourcePumpPhase::Inactive) {
      resumeBootResourcePump(core);
    } else if (bootResourceWaitFields_ != 0) {
      --bootResourceWaitFields_;
      throw FrameCompleted{};
    }
    if (bootResourceWaitResume_ != 0) {
      resumeBootResourceWait(core);
    } else if (!bootEntered_) {
      bootEntered_ = true;
      runtime_.bootInit(core);
    } else {
      runtime_.dispatch(core, native::kFrameLoopResume);
    }
  } catch (const FrameCompleted &) {
    // The finite owner intentionally unwinds through generated func_* wrappers before their manual
    // otattrPop executes. Restore the entry depth at the same ownership transfer.
    core.idiag.otattr_depth = attributionDepth;
    presentation_.finishField(core);
    ++completedFrames_;
    return;
  }

  lucent::error("ctr-frame", "retail execution returned without completing CTR frame {}", frame);
  std::abort();
}

uint32_t CtrFrameDriver::completedFrames() const {
  return completedFrames_;
}

const ProjectionOwner &CtrFrameDriver::projection() const {
  return projection_;
}

const PresentationOwner &CtrFrameDriver::presentation() const {
  return presentation_;
}

void CtrFrameDriver::skipFirstStartupVSync(Core *core) {
  active_->continueAfterVSync(
      *core, native::kStartupGpuInit, native::kStartupGpuInitReturn, native::kAfterFirstStartupVSync, 0u);
}

void CtrFrameDriver::skipSecondStartupVSync(Core *core) {
  active_->continueAfterVSync(
      *core, native::kStartupDisplayInit, native::kStartupDisplayInitReturn, native::kAfterSecondStartupVSync, 0u);
}

void CtrFrameDriver::waitForBootResourceWithoutVSync(Core *core) {
  active_->beginBootResourceWait(*core);
}

void CtrFrameDriver::pumpBootResourceWithoutBusyWait(Core *core) {
  active_->beginBootResourcePump(*core);
}

void CtrFrameDriver::serviceStartupAudioWithoutBusyWait(Core *core) {
  active_->serviceStartupAudio(*core);
}

void CtrFrameDriver::continueStartupAudioLoop(Core *core) {
  active_->resumeStartupAudioLoop(*core);
}

void CtrFrameDriver::skipShutdownVSync(Core *core) {
  active_->continueAfterVSync(
      *core, native::kShutdownDisplay, native::kShutdownDisplayReturn, native::kAfterShutdownVSync, 30u);
}

void CtrFrameDriver::observeVblankCallback(Core *core) {
  active_->frameCallbacks_.observeVblankRegistration(*core, active_->runtime_);
}

void CtrFrameDriver::finishFrameWithoutDebugVSync(Core *core) {
  if (core->r[31] == native::kFrameTimingReturn) {
    active_->completeFrame(*core);
    return;
  }
  if (core->r[31] == native::kFrameTimingQueryReturn) {
    active_->runtime_.runRecompiledSuper(*core, native::kFrameTiming);
    return;
  }
  wrongReturn("frame timing owner", native::kFrameTimingReturn, core->r[31]);
}

void CtrFrameDriver::publishProjection(Core *core) {
  active_->publishMeasuredProjection(*core);
}

void CtrFrameDriver::continueAfterVSync(
    Core &core, uint32_t superAddress, uint32_t expectedReturn, uint32_t continuation, uint32_t mode) {
  if (core.r[31] != expectedReturn) {
    wrongReturn("VSync predecessor", expectedReturn, core.r[31]);
  }
  runtime_.runRecompiledSuper(core, superAddress);
  // Preserve the exact jal/delay-slot effects while omitting only the forbidden guest call.
  core.r[31] = continuation;
  core.r[4] = mode;
  rec_guest_instruction_ticks(&core, 2u);
  runtime_.dispatch(core, continuation);
  lucent::error("ctr-frame", "post-VSync continuation 0x{:08X} returned before a frame boundary", continuation);
  std::abort();
}

void CtrFrameDriver::beginBootResourceWait(Core &core) {
  constexpr uint32_t kResourceSetup = 0x800321B4u;
  constexpr uint32_t kResourceCommit = 0x80031EE4u;
  constexpr uint32_t kBeforeResourceWait = 0x8003E978u;

  const uint32_t originalStack = core.r[29];
  const uint32_t waitMode = core.mem_r32(originalStack + 16u);
  if (waitMode != 0xFFFFFFFFu) {
    runtime_.runRecompiledSuper(core, native::kBootResourceWait);
    return;
  }
  if (bootResourceWaitResume_ != 0 || bootResourceWaitFields_ != 0) {
    lucent::error("ctr-frame", "nested state-zero resource waits are not supported");
    std::abort();
  }

  // Exact 0x80031FDC..0x80032074 path for fifth argument -1 in SCUS_944.26. The generated helper
  // calls remain authoritative; this transcription owns only the caller frame and omits VSync(2).
  core.r[29] -= 72u;
  core.mem_w32(core.r[29] + 52u, core.r[17]);
  core.r[17] = core.mem_r32(core.r[29] + 88u);
  core.mem_w32(core.r[29] + 48u, core.r[16]);
  core.r[16] = core.r[4];
  core.mem_w32(core.r[29] + 56u, core.r[18]);
  core.r[18] = core.r[5];
  core.mem_w32(core.r[29] + 64u, core.r[20]);
  core.r[20] = core.r[6];
  core.mem_w32(core.r[29] + 60u, core.r[19]);
  core.r[19] = core.r[7];
  core.mem_w32(core.r[29] + 68u, core.r[31]);
  rec_guest_instruction_ticks(&core, 13u);
  if (core.r[20] == 0) {
    core.r[31] = 0x80032018u;
    rec_guest_instruction_ticks(&core, 2u);
    runtime_.dispatch(core, kBeforeResourceWait);
  }

  core.r[2] = 0xFFFFFFFFu;
  core.r[2] = 0xFFFFFFFEu;
  rec_guest_instruction_ticks(&core, 3u);
  core.r[4] = core.r[16];
  core.r[5] = 3u;
  core.r[6] = core.r[18];
  core.r[7] = core.r[20];
  core.r[2] = core.r[5];
  core.mem_w32(core.r[29] + 24u, core.r[4]);
  core.mem_w16(core.r[29] + 28u, 0u);
  core.mem_w16(core.r[29] + 30u, static_cast<uint16_t>(core.r[2]));
  core.mem_w32(core.r[29] + 32u, core.r[6]);
  core.mem_w32(core.r[29] + 16u, core.r[19]);
  core.r[31] = 0x80032054u;
  core.mem_w32(core.r[29] + 20u, 0u);
  rec_guest_instruction_ticks(&core, 12u);
  runtime_.dispatch(core, kResourceSetup);
  core.mem_w32(core.r[29] + 36u, core.r[2]);
  core.r[2] = core.mem_r32(core.r[19]);
  core.r[4] = core.r[29] + 24u;
  core.mem_w32(core.r[29] + 44u, 0u);
  core.r[31] = 0x8003206Cu;
  core.mem_w32(core.r[29] + 40u, core.r[2]);
  rec_guest_instruction_ticks(&core, 6u);
  runtime_.dispatch(core, kResourceCommit);

  core.r[31] = native::kBootResourceWaitReturn;
  core.r[4] = 2u;
  rec_guest_instruction_ticks(&core, 2u);
  bootResourceWaitResume_ = native::kBootResourceWaitReturn;
  bootResourceWaitFields_ = 1u;
  throw FrameCompleted{};
}

void CtrFrameDriver::resumeBootResourceWait(Core &core) {
  const uint32_t resume = bootResourceWaitResume_;
  bootResourceWaitResume_ = 0;
  runtime_.dispatch(core, resume);
  const uint32_t caller = core.r[31];
  if (caller != native::kBootResourceWaitFirstCaller && caller != native::kBootResourceWaitSecondCaller &&
      caller != native::kBootResourceWaitRaceCaller) {
    wrongReturn("state-zero resource wait", native::kBootResourceWaitFirstCaller, caller);
  }
  runtime_.dispatch(core, caller);
  if (caller == native::kBootResourceWaitRaceCaller) {
    if (core.r[31] != native::kBootResourceWaitRaceResume) {
      wrongReturn("state-zero race resource suffix", native::kBootResourceWaitRaceResume, core.r[31]);
    }
    runtime_.dispatch(core, native::kBootResourceWaitRaceResume);
    lucent::error("ctr-frame",
                  "state-zero race resource continuation 0x{:08X} returned before a frame boundary",
                  native::kBootResourceWaitRaceResume);
    std::abort();
  }
  lucent::error("ctr-frame", "state-zero resource continuation 0x{:08X} returned before a frame boundary", caller);
  std::abort();
}

void CtrFrameDriver::beginBootResourcePump(Core &core) {
  if (core.r[31] != native::kBootResourcePumpReturn) {
    wrongReturn("state-zero resource pump", native::kBootResourcePumpReturn, core.r[31]);
  }
  if (bootResourcePumpPhase_ != BootResourcePumpPhase::Inactive) {
    lucent::error("ctr-frame", "nested state-zero resource pumps are not supported");
    std::abort();
  }

  // Exact 0x8002DD24..0x8002DD3C setup. The generated resource state machine remains authoritative;
  // only its host-starving do/while ownership moves into the finite driver.
  core.r[29] -= 32u;
  core.r[4] = 33u;
  core.mem_w32(core.r[29] + 24u, core.r[31]);
  core.mem_w8(core.r[28] + 2248u, 0u);
  core.r[31] = 0x8002DD3Cu;
  core.r[5] = core.r[29] + 16u;
  rec_guest_instruction_ticks(&core, 6u);
  runtime_.dispatch(core, native::kBootResourcePumpBegin);
  bootResourcePumpPhase_ = BootResourcePumpPhase::ResourcePoll;
  resumeBootResourcePump(core);
}

void CtrFrameDriver::resumeBootResourcePump(Core &core) {
  if (bootResourcePumpPhase_ == BootResourcePumpPhase::ResourcePoll) {
    if (core.pending_work) {
      servicePendingInterrupts(core);
    }
    core.r[31] = 0x8002DD44u;
    rec_guest_instruction_ticks(&core, 2u);
    runtime_.dispatch(core, native::kBootResourcePumpPoll);
    rec_guest_instruction_ticks(&core, 2u);
    if (core.r[2] == 0u) {
      throw FrameCompleted{};
    }

    core.r[31] = 0x8002DD54u;
    core.r[4] = 28u;
    rec_guest_instruction_ticks(&core, 2u);
    runtime_.dispatch(core, native::kBootResourcePumpCommit);
    bootResourcePumpPhase_ = BootResourcePumpPhase::CommitPoll;
  }

  if (bootResourcePumpPhase_ != BootResourcePumpPhase::CommitPoll) {
    lucent::error("ctr-frame", "state-zero resource pump resumed without a measured phase");
    std::abort();
  }
  if (core.pending_work) {
    servicePendingInterrupts(core);
  }
  core.r[31] = 0x8002DD5Cu;
  rec_guest_instruction_ticks(&core, 2u);
  runtime_.dispatch(core, native::kBootResourcePumpCommitPoll);
  rec_guest_instruction_ticks(&core, 2u);
  if (core.r[2] == 0u) {
    throw FrameCompleted{};
  }
  finishBootResourcePump(core);
}

void CtrFrameDriver::finishBootResourcePump(Core &core) {
  const uint32_t continuation = core.mem_r32(core.r[29] + 24u);
  core.r[31] = continuation;
  core.r[29] += 32u;
  rec_guest_instruction_ticks(&core, 4u);
  bootResourcePumpPhase_ = BootResourcePumpPhase::Inactive;
  if (continuation != native::kBootResourcePumpReturn) {
    wrongReturn("state-zero resource-pump suffix", native::kBootResourcePumpReturn, continuation);
  }
  runtime_.dispatch(core, continuation);
  lucent::error("ctr-frame", "resource-pump continuation 0x{:08X} returned before a frame boundary", continuation);
  std::abort();
}

void CtrFrameDriver::servicePendingInterrupts(Core &core) {
  // A synchronous native transfer can become owed during the immediately preceding generated
  // resource poll. Deliver CTR's measured channel-4 callback before the generic direct-runtime path
  // consumes a completion for which it has no legacy callback-table view, then service all remaining
  // IRQ sources normally.
  dmaCallbacks_.serviceSpu(core, runtime_);
  if (dmaCallbacks_.hasPendingSpu()) {
    throw FrameCompleted{};
  }
  rec_irq_poll(&core);
}

void CtrFrameDriver::resumeStartupAudioLoop(Core &core) {
  // The wrapper for this exact loop first lets Hle::irqPoll finish CTR's custom-exception unwind.
  // A channel-4 completion created inside that unwind is now deliverable, but the direct runtime's
  // generic DMA table is intentionally absent. Deliver one measured callback here. If it starts the
  // next synchronous transfer, yield the native field before generated code can consume it without
  // a callback; hardware would not complete both transfers in the same interrupt either.
  dmaCallbacks_.serviceSpu(core, runtime_);
  if (dmaCallbacks_.hasPendingSpu()) {
    startupAudioWaitResume_ = native::kStartupAudioLoop;
    throw FrameCompleted{};
  }
  runtime_.runRecompiledSuper(core, native::kStartupAudioLoop);
}

void CtrFrameDriver::serviceStartupAudio(Core &core) {
  const uint32_t caller = core.r[31];
  runtime_.runRecompiledSuper(core, native::kStartupAudioService);
  if (caller != native::kStartupAudioServiceReturn) {
    return;
  }
  if (core.mem_r32(native::kStartupAudioWaitState) == 0u) {
    return;
  }
  if (startupAudioWaitResume_ != 0u) {
    lucent::error("ctr-frame", "nested state-zero audio waits are not supported");
    std::abort();
  }
  startupAudioWaitResume_ = native::kStartupAudioServiceReturn;
  throw FrameCompleted{};
}

void CtrFrameDriver::resumeStartupAudioWait(Core &core) {
  const uint32_t resume = startupAudioWaitResume_;
  startupAudioWaitResume_ = 0u;
  runtime_.dispatch(core, resume);
  lucent::error("ctr-frame", "state-zero audio continuation 0x{:08X} returned before a frame boundary", resume);
  std::abort();
}

void CtrFrameDriver::completeFrame(Core &core) {
  if (core.r[31] != native::kFrameTimingReturn) {
    wrongReturn("frame timing super", native::kFrameTimingReturn, core.r[31]);
  }

  runtime_.runRecompiledSuper(core, native::kFrameTiming);

  // Exact 0x8003785C..0x80037880 bridge from SCUS_944.26. It retains the result store, debug-flag
  // read, and guest instruction accounting, but deliberately omits the conditional VSync(0) call.
  core.r[3] = core.mem_r32(core.r[28] + 832u);
  core.mem_w32(core.r[20] + 7388u, core.r[2]);
  core.r[2] = core.mem_r32(core.r[3] + 9580u);
  core.r[2] &= 4096u;
  const bool debugVSync = core.r[2] != 0;
  rec_guest_instruction_ticks(&core, debugVSync ? 9u : 7u);
  if (debugVSync) {
    // These are the jal/delay-slot register effects at 0x80037878/0x8003787C. The host still owns
    // timing, so the call itself is deliberately absent.
    core.r[31] = native::kFrameSuffix;
    core.r[4] = 0u;
  }

  frameSuffixPending_ = true;
  resumeFrameSuffix(core);
}

bool CtrFrameDriver::frameSuffixIsWaiting(Core &core) const {
  const uint32_t gameState = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  if (gameState == 0u) {
    return false;
  }
  const bool drawPending = core.mem_r8(gameState + native::kDrawSyncPendingOffset) != 0u;
  const bool fieldPending = static_cast<int32_t>(core.mem_r32(core.r[28] + native::kFrameWaitFieldsGpOffset)) > 0;
  const uint32_t callbackCount = core.mem_r32(gameState + native::kFrameCallbackCountOffset);
  return (drawPending || fieldPending) && callbackCount < 7u;
}

void CtrFrameDriver::resumeFrameSuffix(Core &core) {
  if (!frameSuffixPending_) {
    lucent::error("ctr-frame", "frame suffix resumed without an owned continuation");
    std::abort();
  }
  if (frameSuffixIsWaiting(core)) {
    throw FrameCompleted{};
  }

  runtime_.dispatch(core, native::kFrameSuffix);
  if (core.r[31] != native::kFrameLoopResume) {
    wrongReturn("frame suffix", native::kFrameLoopResume, core.r[31]);
  }
  frameSuffixPending_ = false;
  throw FrameCompleted{};
}

void CtrFrameDriver::publishMeasuredProjection(Core &core) {
  const uint32_t returnAddress = core.r[31];
  if (returnAddress != native::kProjectionReturnLensflare && returnAddress != native::kProjectionReturnState &&
      returnAddress != native::kProjectionReturnOverlay) {
    lucent::error("ctr-projection",
                  "projection owner reached from unmeasured return 0x{:08X}; expected one of "
                  "0x{:08X}/0x{:08X}/0x{:08X}",
                  returnAddress,
                  native::kProjectionReturnLensflare,
                  native::kProjectionReturnState,
                  native::kProjectionReturnOverlay);
    std::abort();
  }
  projection_.publish(core, [this](Core &projectionCore) {
    runtime_.runRecompiledSuper(projectionCore, native::kProjectionProducer);
  });
}

} // namespace ctr
