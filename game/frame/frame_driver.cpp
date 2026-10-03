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

// The three extracted VSync callsites, as data: the function retail calls from, the one return address
// that identifies the call, the instruction after it, and the mode retail passes across (VSync's a0:
// 0 for the two startup waits, 30 for the shutdown wait's measured length).
constexpr VsyncBridge kStartupGpuVSyncBridge{
    native::kStartupGpuInit, native::kStartupGpuInitReturn, native::kAfterFirstStartupVSync, 0u};
constexpr VsyncBridge kStartupDisplayVSyncBridge{
    native::kStartupDisplayInit, native::kStartupDisplayInitReturn, native::kAfterSecondStartupVSync, 0u};
constexpr VsyncBridge kShutdownVSyncBridge{
    native::kShutdownDisplay, native::kShutdownDisplayReturn, native::kAfterShutdownVSync, 30u};

} // namespace

CtrFrameDriver::CtrFrameDriver(CtrRuntime &runtime)
    : runtime_(runtime), renderListDiagnostic_(cfg_dbg("ctr-render-list") != 0),
      field_(renderListDiagnostic_, presentation_), resourceLoad_(runtime, field_),
      resourcePump_(runtime, dmaCallbacks_, field_), startupAudio_(runtime, dmaCallbacks_, field_),
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
  // The projection census denominator, taken BEFORE the field's work so a field that faults still
  // counts as one the owner was live for.
  projection_.beginField();
  // Armed for the same window as the address overrides: guest GTE ops only happen inside this field.
  const ScopedGteProjectionObservation geometryObservation(core, geometryProjection_);
  // The widening plan is resolved from the guest's OWN published view, inside
  // ProjectionOwner::publish, never from a display register at boot.
  projection_.setPlanSource([this](Core &projectionCore, const GuestViewProjection &view) {
    return widescreen_.planFor(projectionCore, view);
  });
  // The geometry owner READS the plan this one resolves; it never resolves a second one. Rebound every
  // field so a null owner here is a visible census refusal instead of a silent "0 widened".
  geometryProjection_.setProjectionOwner(&projection_);

  // Every guest address the field owns, and the name each is installed under. Which owners a
  // non-observing field drops is a property of the rows, not of their order.
  const std::array<FieldOverrideBinding, 11> fieldOverrides{{
      {native::kStartupGpuInit, "startup GPU VSync owner", onStartupGpuVSync},
      {native::kStartupDisplayInit, "startup display VSync owner", onStartupDisplayVSync},
      {native::kBootResourceWait, "boot resource wait owner", onBootResourceWait},
      {native::kBootResourcePump, "boot resource pump owner", onBootResourcePump},
      {native::kStartupAudioService, "startup audio wait owner", onStartupAudioService},
      {native::kStartupAudioLoop, "startup audio loop owner", onStartupAudioLoop},
      {native::kShutdownDisplay, "shutdown VSync owner", onShutdownVSync},
      {native::kVblankCallbackInstall, "vblank callback owner", onVblankCallback},
      {native::kFrameTiming, "frame timing owner", onFrameTiming},
      {native::kProjectionProducer, "projection owner", onProjectionProducer},
      {native::kRenderListPublisher, "render-list observer", onRenderListPublisher, true},
  }};
  const FieldOverrideScope overrides(core, fieldOverrides, renderListDiagnostic_.enabled());
  field_.beginField();

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
  discReadOwner_.deliverPending(core, runtime_);
  dmaCallbacks_.serviceSpu(core, runtime_);
  if (dmaCallbacks_.hasPendingSpu(core)) {
    field_.request(core);
  }

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

// Serves whichever continuation the previous field owed, if any. This order is the driver's whole
// continuation ladder, and only one of them can ever be owed at a time because each ends its field
// rather than returning into the ladder.
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
                  "retail execution left frame {} at 0x{:08X} with {}",
                  frame,
                  execution->guestPc,
                  psx::cpu::executionExitName(execution->reason));
  } else {
    lucent::error("ctr-frame", "retail execution returned without completing CTR frame {}", frame);
  }
  // The projection census is reported HERE rather than at a clean shutdown: the run that needs the
  // number most is the one that dies.
  projection_.reportCensus();
  geometryProjection_.reportCensus();
  core.guestCallCensus().log("after CTR field refusal");
  std::abort();
}

psx::cpu::ExecutionResult CtrFrameDriver::dispatchField(Core &core, uint32_t frame, uint32_t entry) {
  auto result = runtime_.dispatch(core, entry);
  while (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    // THIS IS NOT A GUEST CALL and does not enter `psx::cpu::ResumableGuestCall`: a field has no
    // return boundary to latch into `r[31]`, and its terminal exit is the typed frame boundary rather
    // than a guest return. What is borrowed from the shared contract is the progress rule: a budget
    // exit that resumed at the same PC and consumed cycles is the same field's finite quantity, while
    // one that consumed nothing made no progress and is refused here instead of being spun on.
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

RenderListBoundaryDiagnostic &CtrFrameDriver::renderListDiagnostic() {
  return renderListDiagnostic_;
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

void CtrFrameDriver::onStartupAudioLoop(Core *core) {
  ctrFrameDriver(*core).startupAudio_.resumeLoop(*core);
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

void CtrFrameDriver::onRenderListPublisher(Core *core) {
  ctrFrameDriver(*core).observePublishedRenderList(*core);
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
  // The three measured return addresses ARE the identity here, so the classification is the same
  // comparison the refusal used to make. It is a function rather than an inline chain because the
  // census tallies per source and a second, differently-spelled list of the three addresses would
  // be exactly the duplication that lets a fourth caller in unnoticed.
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

void CtrFrameDriver::observePublishedRenderList(Core &core) {
  renderListDiagnostic_.observePublication(core, [this](Core &publisherCore) {
    runtime_.callOriginalToReturn(publisherCore, native::kRenderListPublisher, "CTR render-list publisher");
  });
}

} // namespace ctr
