#include "frame_driver.h"

#include "cfg.h"
#include "core.h"
#include "ctr_runtime.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "field_override_scope.h"
#include "game.h"
#include "native_ownership.h"
#include "retail_return.h"
#include "vsync_bridge.h"

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

CtrFrameDriver::CtrFrameDriver(CtrRuntime &runtime)
    : runtime_(runtime), presentation_(runtime.sceneCut()), field_(presentation_), resourceLoad_(runtime, field_),
      resourcePump_(runtime, field_), startupAudio_(runtime, field_), spuDmaRegistration_(runtime),
      frameSuffix_(runtime, field_) {}

void CtrFrameDriver::stepFrame(Core &core, uint32_t frame) {
  if (!core.game) {
    lucent::error("ctr-frame", "frame {} has no bound Game", frame);
    std::abort();
  }
  if (frame != field_.completedFields()) {
    lucent::error(
        "ctr-frame", "non-sequential frame {} requested after {} completed field(s)", frame, field_.completedFields());
    std::abort();
  }
  budgetExitsThisField_ = 0;
  // Armed for the same window as the address overrides, since guest GTE ops only run inside a field.
  const ScopedGteProjectionObservation geometryObservation(core, geometryProjection_);
  // The widening plan is resolved from the guest's own published view in ProjectionOwner::publish.
  projection_.setPlanSource([this](Core &projectionCore, const GuestViewProjection &view) {
    return widescreen_.planFor(projectionCore, view);
  });
  // Rebound every field so a null owner is refused instead of silently widening nothing.
  geometryProjection_.setProjectionOwner(&projection_);

  const std::array<FieldOverrideBinding, 10> fieldOverrides{{
      {native::kStartupGpuInit, "startup GPU VSync owner", onStartupGpuVSync},
      {native::kStartupDisplayInit, "startup display VSync owner", onStartupDisplayVSync},
      {native::kBootResourceWait, "boot resource wait owner", onBootResourceWait},
      {native::kBootResourcePump, "boot resource pump owner", onBootResourcePump},
      {native::kStartupAudioService, "startup audio wait owner", onStartupAudioService},
      {native::kShutdownDisplay, "shutdown VSync owner", onShutdownVSync},
      {native::kVblankCallbackInstall, "vblank callback owner", onVblankCallback},
      {native::kFrameTiming, "frame timing owner", onFrameTiming},
      {native::kProjectionProducer, "projection owner", onProjectionProducer},
      {native::kDmaCallback, "DMA callback registration owner", onDmaCallbackRegistration},
  }};
  const FieldOverrideScope overrides(core, fieldOverrides);
  field_.beginField();

  core.game->timing.logicFrame = frame;
  core.rsub.otAttr.beginLogicFrame(frame);
  core.game->timing.frameTick();
  frameCallbacks_.deliverField(core, runtime_);
  core.game->pad.serviceFrame();
  // XA playback is pull-driven; state zero waits on the clip task at 0x8008D708.
  core.game->spu_audio.frame();
  // Retail delivers this from the CD interrupt, and the loader stores its buffer only after the read returns.
  discReadOwner_.deliverPending(core, runtime_);

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
    execution = dispatchField(core, frame, runtime_.bootTarget());
  } else {
    execution = dispatchField(core, frame, native::kFrameLoopResume);
  }
  if (field_.pending(core) || (execution && execution->reason == psx::cpu::ExecutionExitReason::FrameBoundary)) {
    field_.finishField(core, frame);
    return;
  }

  refuseUnfinishedField(core, frame, execution);
}

// The continuation ladder; only one continuation can be owed at a time.
void CtrFrameDriver::resumeOwedContinuation(Core &core) {
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

void CtrFrameDriver::refuseUnfinishedField(Core &core,
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

psx::cpu::ExecutionResult CtrFrameDriver::dispatchField(Core &core, uint32_t frame, uint32_t entry) {
  auto result = runtime_.dispatch(core, entry);
  while (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    // A field has no return boundary: a budget exit that consumed cycles is the same
    // field, one that consumed none made no progress and is refused.
    if (result.guestPc != core.pc || result.cycles == 0) {
      lucent::error("ctr-frame",
                    "frame {} cannot resume budget exit {} at 0x{:08X}: Core PC=0x{:08X}, cycles={}",
                    frame,
                    budgetExitsThisField_ + 1u,
                    result.guestPc,
                    core.pc,
                    result.cycles);
      std::abort();
    }
    ++budgetExitsThisField_;
    // Retail raises the CD interrupt asynchronously; a guest polling for it never reaches a field start.
    discReadOwner_.deliverIfDue(core, runtime_);
    result = runtime_.dispatch(core, result.guestPc);
  }
  return result;
}

uint32_t CtrFrameDriver::completedFrames() const {
  return field_.completedFields();
}

uint64_t CtrFrameDriver::budgetExitsForLastField() const {
  return budgetExitsThisField_;
}

const ProjectionOwner &CtrFrameDriver::projection() const {
  return projection_;
}

const CtrWidescreen &CtrFrameDriver::widescreen() const {
  return widescreen_;
}

const PresentationOwner &CtrFrameDriver::presentation() const {
  return presentation_;
}

DiscReadOwner &CtrFrameDriver::discReadOwner() {
  return discReadOwner_;
}

CtrFrameDriver &ctrFrameDriver(Core &core) {
  if (!core.game || !core.game->frameDriver) {
    lucent::error("ctr-frame", "guest override has no Core with a bound CTR frame driver");
    std::abort();
  }
  auto *driver = dynamic_cast<CtrFrameDriver *>(core.game->frameDriver.get());
  if (!driver) {
    lucent::error("ctr-frame", "guest override reached a frame driver this title does not own");
    std::abort();
  }
  return *driver;
}

void CtrFrameDriver::onStartupGpuVSync(Core *core) {
  CtrFrameDriver &driver = ctrFrameDriver(*core);
  runVsyncBridge(*core, driver.runtime_, kStartupGpuVSyncBridge);
}

void CtrFrameDriver::onStartupDisplayVSync(Core *core) {
  CtrFrameDriver &driver = ctrFrameDriver(*core);
  runVsyncBridge(*core, driver.runtime_, kStartupDisplayVSyncBridge);
}

void CtrFrameDriver::onShutdownVSync(Core *core) {
  CtrFrameDriver &driver = ctrFrameDriver(*core);
  runVsyncBridge(*core, driver.runtime_, kShutdownVSyncBridge);
}

void CtrFrameDriver::onBootResourceWait(Core *core) {
  ctrFrameDriver(*core).resourceLoad_.begin(*core);
}

void CtrFrameDriver::onBootResourcePump(Core *core) {
  ctrFrameDriver(*core).resourcePump_.begin(*core);
}

void CtrFrameDriver::onStartupAudioService(Core *core) {
  ctrFrameDriver(*core).startupAudio_.service(*core);
}

void CtrFrameDriver::onDmaCallbackRegistration(Core *core) {
  ctrFrameDriver(*core).spuDmaRegistration_.service(*core);
}

void CtrFrameDriver::onVblankCallback(Core *core) {
  CtrFrameDriver &driver = ctrFrameDriver(*core);
  driver.frameCallbacks_.observeVblankRegistration(*core, driver.runtime_);
}

void CtrFrameDriver::onFrameTiming(Core *core) {
  CtrFrameDriver &driver = ctrFrameDriver(*core);
  if (core->r[31] == native::kFrameTimingReturn) {
    driver.frameSuffix_.completeFieldTiming(*core);
    return;
  }
  if (core->r[31] == native::kFrameTimingQueryReturn) {
    driver.runtime_.callOriginalToReturn(*core, native::kFrameTiming, "CTR frame-timing query");
    return;
  }
  refuseUnexpectedRetailReturn("frame timing owner", native::kFrameTimingReturn, core->r[31]);
}

void CtrFrameDriver::onProjectionProducer(Core *core) {
  ctrFrameDriver(*core).publishMeasuredProjection(*core);
}

void CtrFrameDriver::publishMeasuredProjection(Core &core) {
  const uint32_t returnAddress = core.r[31];
  const ProjectionOwner::Source source = classifyProjectionSource(returnAddress);
  projection_.publish(
      core,
      [this](Core &projectionCore) {
        runtime_.callOriginalToReturn(projectionCore, native::kProjectionProducer, "CTR projection producer");
      },
      source);
}

ProjectionOwner::Source CtrFrameDriver::classifyProjectionSource(uint32_t returnAddress) {
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
