#include "frame/frame_driver.h"

#include "core.h"
#include "execution/guest_turns.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "frame/field_override_scope.h"
#include "frame/retail_return.h"
#include "frame/vsync_bridge.h"
#include "game.h"
#include "runtime/session.h"
#include "title/native_ownership.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// The three extracted VSync callsites; the shutdown wait passes 30 and the startup waits 0.
constexpr VsyncBridge kStartupGpuVSyncBridge{
    native::kStartupGpuInit, native::kStartupGpuInitReturn, native::kAfterFirstStartupVSync, 0u};
constexpr VsyncBridge kStartupDisplayVSyncBridge{
    native::kStartupDisplayInit, native::kStartupDisplayInitReturn, native::kAfterSecondStartupVSync, 0u};
constexpr VsyncBridge kShutdownVSyncBridge{
    native::kShutdownDisplay, native::kShutdownDisplayReturn, native::kAfterShutdownVSync, 30u};

} // namespace

FrameDriver::FrameDriver(Game &game)
    : discReadOwner_(session(game.core).discRead), presentation_(session(game.core).sceneCut), field_(presentation_),
      resourceLoad_(field_), resourcePump_(field_), startupAudio_(field_), frameSuffix_(field_) {}

void FrameDriver::stepFrame(Core &core, uint32_t frame) {
  if (!core.game) {
    lucent::error("ctr-frame", "frame {} has no bound Game", frame);
    std::abort();
  }
  if (lastFrame_ && frame != *lastFrame_ + 1u) {
    lucent::error("ctr-frame", "non-sequential frame {} requested after frame {}", frame, *lastFrame_);
    std::abort();
  }
  lastFrame_ = frame;
  budgetExitsThisField_ = 0;
  // Armed for the same window as the address overrides, since guest GTE ops only run inside a field.
  const ScopedGteProjectionObservation geometryObservation(core, geometryProjection_);
  // The widening plan is resolved from the guest's own published view in ProjectionOwner::publish.
  projection_.setPlanSource([this](Core &projectionCore, const GuestViewProjection &view) {
    return widescreen_.planFor(projectionCore, view);
  });
  // Rebound every field so a null owner is refused instead of silently widening nothing.
  geometryProjection_.setProjectionOwner(&projection_);

  const std::array<FieldOverrideBinding, 9> fieldOverrides{{
      {native::kStartupGpuInit, "startup GPU VSync owner", onStartupGpuVSync},
      {native::kStartupDisplayInit, "startup display VSync owner", onStartupDisplayVSync},
      {native::kBootResourceWait, "boot resource wait owner", onBootResourceWait},
      {native::kBootResourcePump, "boot resource pump owner", onBootResourcePump},
      {native::kStartupAudioService, "startup audio wait owner", onStartupAudioService},
      {native::kShutdownDisplay, "shutdown VSync owner", onShutdownVSync},
      {native::kFrameTiming, "frame timing owner", onFrameTiming},
      {native::kProjectionProducer, "projection owner", onProjectionProducer},
      {native::kDmaCallback, "DMA callback registration owner", onDmaCallbackRegistration},
  }};
  const FieldOverrideScope overrides(core, fieldOverrides);
  field_.beginField();

  core.game->timing.logicFrame = frame;
  core.rsub.otAttr.beginLogicFrame(frame);
  core.game->timing.frameTick();
  frameCallbacks_.deliverField(core);
  core.game->pad.serviceFrame();
  // XA playback is pull-driven; state zero waits on the clip task at 0x8008D708.
  core.game->spu_audio.frame();
  // Retail delivers this from the CD interrupt, and the loader stores its buffer only after the read returns.
  discReadOwner_.deliverPending(core);

  if (!field_.pending(core)) {
    resumeOwedContinuation(core);
  }
  if (field_.pending(core)) {
    field_.finishField(core, frame);
    return;
  }

  std::optional<psx::cpu::ExecutionResult> execution;
  if (resourceLoad_.ownsSuffix()) {
    resourceLoad_.resume(core);
  } else if (!bootEntered_) {
    bootEntered_ = true;
    execution = dispatchField(core, core.pc);
  } else {
    warp_.apply(core);
    execution = dispatchField(core, native::kFrameLoopResume);
  }
  if (field_.pending(core) || (execution && execution->reason == psx::cpu::ExecutionExitReason::FrameBoundary)) {
    field_.finishField(core, frame);
    return;
  }

  refuseUnfinishedField(core, frame, execution);
}

// The continuation ladder; only one continuation can be owed at a time.
void FrameDriver::resumeOwedContinuation(Core &core) {
  if (frameSuffix_.isPending()) {
    frameSuffix_.resume(core);
  } else if (startupAudio_.ownsContinuation()) {
    startupAudio_.resume(core);
  } else if (resourcePump_.isActive()) {
    resourcePump_.resume(core);
  } else if (resourceLoad_.consumeWaitedField()) {
    field_.request(core);
  }
}

void FrameDriver::refuseUnfinishedField(Core &core,
                                        uint32_t frame,
                                        const std::optional<psx::cpu::ExecutionResult> &execution) {
  if (execution) {
    lucent::error("ctr-frame",
                  "retail execution left frame {} at 0x{:08X} with {}: {}",
                  frame,
                  execution->guestPc,
                  psx::cpu::executionExitName(execution->reason),
                  execution->detail);
  } else {
    lucent::error("ctr-frame", "retail execution returned without completing CTR frame {}", frame);
  }
  std::abort();
}

psx::cpu::ExecutionResult FrameDriver::dispatchField(Core &core, uint32_t entry) {
  auto result = guest::dispatch(core, entry);
  while (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    // A field has no return boundary: a budget exit that consumed cycles is the same
    // field, one that consumed none made no progress and is refused.
    if (result.guestPc != core.pc || result.cycles == 0) {
      lucent::error("ctr-frame",
                    "frame {} cannot resume budget exit {} at 0x{:08X}: Core PC=0x{:08X}, cycles={}",
                    core.game->timing.logicFrame,
                    budgetExitsThisField_ + 1u,
                    result.guestPc,
                    core.pc,
                    result.cycles);
      std::abort();
    }
    ++budgetExitsThisField_;
    // Retail raises the CD interrupt asynchronously; a guest polling for it never reaches a field start.
    discReadOwner_.deliverIfDue(core);
    result = guest::dispatch(core, result.guestPc);
  }
  return result;
}

uint32_t FrameDriver::completedFrames() const {
  return field_.completedFields();
}

uint64_t FrameDriver::budgetExitsForLastField() const {
  return budgetExitsThisField_;
}

const ProjectionOwner &FrameDriver::projection() const {
  return projection_;
}

const WidescreenOwner &FrameDriver::widescreen() const {
  return widescreen_;
}

const PresentationOwner &FrameDriver::presentation() const {
  return presentation_;
}

std::string FrameDriver::armWarp(Core &core, const char *line) {
  return warp_.arm(line, core);
}

FrameDriver &FrameDriver::from(Core &core) {
  if (!core.game || !core.game->frameDriver) {
    lucent::error("ctr-frame", "guest override has no Core with a bound CTR frame driver");
    std::abort();
  }
  auto *driver = dynamic_cast<FrameDriver *>(core.game->frameDriver.get());
  if (!driver) {
    lucent::error("ctr-frame", "guest override reached a frame driver this title does not own");
    std::abort();
  }
  return *driver;
}

void FrameDriver::onStartupGpuVSync(Core *core) {
  runVsyncBridge(*core, kStartupGpuVSyncBridge);
}

void FrameDriver::onStartupDisplayVSync(Core *core) {
  runVsyncBridge(*core, kStartupDisplayVSyncBridge);
}

void FrameDriver::onShutdownVSync(Core *core) {
  runVsyncBridge(*core, kShutdownVSyncBridge);
}

void FrameDriver::onBootResourceWait(Core *core) {
  from(*core).resourceLoad_.begin(*core);
}

void FrameDriver::onBootResourcePump(Core *core) {
  from(*core).resourcePump_.begin(*core);
}

void FrameDriver::onStartupAudioService(Core *core) {
  from(*core).startupAudio_.service(*core);
}

void FrameDriver::onDmaCallbackRegistration(Core *core) {
  from(*core).spuDmaRegistration_.service(*core);
}

void FrameDriver::onFrameTiming(Core *core) {
  FrameDriver &driver = from(*core);
  if (core->r[31] == native::kFrameTimingReturn) {
    driver.frameSuffix_.completeFieldTiming(*core);
    return;
  }
  if (core->r[31] == native::kFrameTimingQueryReturn) {
    guest::callOriginalToReturn(*core, native::kFrameTiming, "CTR frame-timing query");
    return;
  }
  refuseUnexpectedRetailReturn("frame timing owner", native::kFrameTimingReturn, core->r[31]);
}

void FrameDriver::onProjectionProducer(Core *core) {
  from(*core).publishMeasuredProjection(*core);
}

void FrameDriver::publishMeasuredProjection(Core &core) {
  const uint32_t returnAddress = core.r[31];
  const ProjectionOwner::Source source = classifyProjectionSource(returnAddress);
  projection_.publish(
      core,
      [](Core &projectionCore) {
        guest::callOriginalToReturn(projectionCore, native::kProjectionProducer, "CTR projection producer");
      },
      source);
}

ProjectionOwner::Source FrameDriver::classifyProjectionSource(uint32_t returnAddress) {
  // The three return addresses are the identity; this is the single list of them.
  if (returnAddress == native::kProjectionReturnLensflare) {
    return ProjectionOwner::Source::LensFlare;
  }
  if (returnAddress == native::kProjectionReturnState) {
    return ProjectionOwner::Source::StateZero;
  }
  if (returnAddress == native::kProjectionReturnOverlay) {
    return ProjectionOwner::Source::Overlay;
  }
  lucent::error("ctr-projection",
                "projection owner reached from unmeasured return 0x{:08X}; expected one of "
                "0x{:08X}/0x{:08X}/0x{:08X}",
                returnAddress,
                native::kProjectionReturnLensflare,
                native::kProjectionReturnState,
                native::kProjectionReturnOverlay);
  std::abort();
}

} // namespace ctr
