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

// The three extracted VSync callsites, as data. Each is the function retail calls from, the one
// return address that identifies the call, the instruction after it, and the mode retail passes
// across. `modeArgument` is VSync's a0: 0 for the startup and shutdown waits' immediate form, 30
// for the shutdown wait's measured length.
constexpr VsyncBridge kStartupGpuVSyncBridge{
    native::kStartupGpuInit, native::kStartupGpuInitReturn, native::kAfterFirstStartupVSync, 0u};
constexpr VsyncBridge kStartupDisplayVSyncBridge{
    native::kStartupDisplayInit, native::kStartupDisplayInitReturn, native::kAfterSecondStartupVSync, 0u};
constexpr VsyncBridge kShutdownVSyncBridge{
    native::kShutdownDisplay, native::kShutdownDisplayReturn, native::kAfterShutdownVSync, 30u};

// One live CTR frame driver at a time. The override entry points are plain function pointers with no
// user data, so they reach their owner through this claim; a nested driver would make them ambiguous
// rather than merely redundant.
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

} // namespace

CtrFrameDriver *CtrFrameDriver::active_ = nullptr;

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
  // The projection census denominator. Taken BEFORE the field's work so a field that faults still
  // counts as a field the owner was live for, which is what makes "0 of N" honest.
  projection_.beginField();
  // Armed for the same window as the address overrides, and for the same reason: guest GTE
  // ops only happen while guest code runs inside this field.
  const ScopedGteProjectionObservation geometryObservation(core, geometryProjection_);
  // The widening plan is resolved from the guest's OWN published view, on the first publication
  // (inside ProjectionOwner::publish), not from a display register at boot. That is what removes the
  // ordering hazard: `s_disp_w` read 320 on the field a boot-time latch saw and 512 on the first
  // present, and a plan built from 320 is NARROWER than the real 512-dot picture.
  projection_.setPlanSource([this](Core &projectionCore, const GuestViewProjection &view) {
    return widescreen_.planFor(projectionCore, view);
  });
  // The geometry owner READS the plan this one resolves; it does not resolve a second one. Bound on
  // every field rather than once at construction so the two owners cannot be left pointing at
  // different plans by a future edit, and so a null owner here is a visible census refusal instead of
  // a silent "0 widened" that reads like a title with no 3D geometry.
  geometryProjection_.setProjectionOwner(&projection_);

  // Every guest address the field owns, and the name each is installed under. Only the last of them
  // is debug-only, and it says so on its own row: which owners a non-observing field drops is a
  // property of the rows, not of their order.
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
  const FieldOverrideScope overrides(runtime_, core, fieldOverrides, renderListDiagnostic_.enabled());
  const ScopedActiveDriver active(active_, *this);
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

// Serves whichever continuation the previous field owed, if any. The order is the driver's whole
// continuation ladder: an owed suffix first, then the startup audio wait, then the resource pump,
// then the field the startup resource load was waiting out. Only one of them can be owed at a time,
// because each one ends its field rather than returning into the ladder.
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
  // A run that cannot finish still measured something, and the projection census is the one
  // measurement that decides whether a native producer has any pre-GTE state to consume at all.
  // Reporting it HERE rather than at a clean shutdown is deliberate: the run that needs the number
  // most is the one that dies, and a report attached to orderly exit would be silent exactly then.
  projection_.reportCensus();
  geometryProjection_.reportCensus();
  std::abort();
}

psx::cpu::ExecutionResult CtrFrameDriver::dispatchField(Core &core, uint32_t frame, uint32_t entry) {
  auto result = runtime_.dispatch(core, entry);
  while (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    // An ordinary budget exit resumes at the same PC it stopped on and consumed cycles, so it is a
    // finite quantity of one field rather than a new field. An exit at a different PC, or one that
    // consumed nothing, is not that: it would spin the host with no progress, so it is refused here
    // instead of being given a turn ceiling a long finite render list could trip.
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

void CtrFrameDriver::onStartupGpuVSync(Core *core) {
  runVsyncBridge(*core, active_->runtime_, kStartupGpuVSyncBridge);
}

void CtrFrameDriver::onStartupDisplayVSync(Core *core) {
  runVsyncBridge(*core, active_->runtime_, kStartupDisplayVSyncBridge);
}

void CtrFrameDriver::onShutdownVSync(Core *core) {
  runVsyncBridge(*core, active_->runtime_, kShutdownVSyncBridge);
}

void CtrFrameDriver::onBootResourceWait(Core *core) {
  active_->resourceLoad_.begin(*core);
}

void CtrFrameDriver::onBootResourcePump(Core *core) {
  active_->resourcePump_.begin(*core);
}

void CtrFrameDriver::onStartupAudioService(Core *core) {
  active_->startupAudio_.service(*core);
}

void CtrFrameDriver::onStartupAudioLoop(Core *core) {
  active_->startupAudio_.resumeLoop(*core);
}

void CtrFrameDriver::onVblankCallback(Core *core) {
  active_->frameCallbacks_.observeVblankRegistration(*core, active_->runtime_);
}

void CtrFrameDriver::onFrameTiming(Core *core) {
  if (core->r[31] == native::kFrameTimingReturn) {
    active_->frameSuffix_.completeFieldTiming(*core);
    return;
  }
  if (core->r[31] == native::kFrameTimingQueryReturn) {
    active_->runtime_.callOriginalToReturn(*core, native::kFrameTiming, "CTR frame-timing query");
    return;
  }
  refuseUnexpectedRetailReturn("frame timing owner", native::kFrameTimingReturn, core->r[31]);
}

void CtrFrameDriver::onProjectionProducer(Core *core) {
  active_->publishMeasuredProjection(*core);
}

void CtrFrameDriver::onRenderListPublisher(Core *core) {
  active_->observePublishedRenderList(*core);
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
