#pragma once

#include "async_disc_owner.h"
#include "dma_callback_owner.h"
#include "execution_exit.h"
#include "field_boundary.h"
#include "frame_callback_owner.h"
#include "frame_suffix.h"
#include "game_runtime.h"
#include "geometry_projection_owner.h"
#include "presentation_owner.h"
#include "projection_owner.h"
#include "render_list_boundary_diagnostic.h"
#include "startup_audio_wait.h"
#include "startup_resource_load.h"
#include "startup_resource_pump.h"
#include "widescreen_owner.h"

#include <cstdint>
#include <optional>

namespace ctr {

class CtrRuntime;

// Owns one finite CTR state-3 frame at a time, and composes the owners that make one possible.
//
// Lightrec executes retail code on both sides of each extracted VSync callsite; title-local bridges
// resume at identity-gated re-entry points without ever invoking guest libetc VSync. This class is
// the ladder that decides, for the field it has entered, which continuation is owed: a pending
// frame boundary, the retail frame suffix, a startup audio wait, a startup resource pump, a startup
// resource load, or the field's own entry. The owners named above each own one of those answers;
// the driver only chooses between them and dispatches the field's entry.
class CtrFrameDriver final : public FrameDriver {
public:
  explicit CtrFrameDriver(CtrRuntime &runtime);

  void stepFrame(Core &core, uint32_t frame) override;

  [[nodiscard]] uint32_t completedFrames() const;
  [[nodiscard]] uint64_t budgetExitsForLastField() const;
  [[nodiscard]] const ProjectionOwner &projection() const;
  // The second projection application point: the guest's own geometry submitters publish
  // the same triple at ten measured sites the descriptor publication never reaches.
  [[nodiscard]] const CtrGeometryProjectionOwner &geometryProjection() const;
  [[nodiscard]] const CtrWidescreen &widescreen() const;
  [[nodiscard]] const PresentationOwner &presentation() const;
  [[nodiscard]] DiscReadOwner &discReadOwner();

private:
  // The frame's guest overrides are plain function pointers with no user data, so each callsite
  // needs its own entry point. These are those entry points; each names one guest operation and
  // does nothing but hand the core to the owner that owns it.
  static void onStartupGpuVSync(Core *core);
  static void onStartupDisplayVSync(Core *core);
  static void onBootResourceWait(Core *core);
  static void onBootResourcePump(Core *core);
  static void onStartupAudioService(Core *core);
  static void onStartupAudioLoop(Core *core);
  static void onShutdownVSync(Core *core);
  static void onVblankCallback(Core *core);
  static void onFrameTiming(Core *core);
  static void onProjectionProducer(Core *core);
  static void onRenderListPublisher(Core *core);

  // The continuation ladder and the field's own dispatch. `dispatchField` is the one place an
  // ordinary budget exit may be resumed, because it is the one place that decides an exit is the
  // same field's finite quantity rather than the start of another one.
  void resumeOwedContinuation(Core &core);
  void refuseUnfinishedField(Core &core, uint32_t frame, const std::optional<psx::cpu::ExecutionResult> &execution);
  [[nodiscard]] psx::cpu::ExecutionResult dispatchField(Core &core, uint32_t frame, uint32_t entry);
  void publishMeasuredProjection(Core &core);
  [[nodiscard]] static ProjectionOwner::Source classifyProjectionSource(uint32_t returnAddress);
  void observePublishedRenderList(Core &core);

  static CtrFrameDriver *active_;

  CtrRuntime &runtime_;
  DiscReadOwner discReadOwner_;
  DmaCallbackOwner dmaCallbacks_;
  FrameCallbackOwner frameCallbacks_;
  ProjectionOwner projection_;
  CtrGeometryProjectionOwner geometryProjection_;
  CtrWidescreen widescreen_;
  PresentationOwner presentation_;
  RenderListBoundaryDiagnostic renderListDiagnostic_;
  FieldBoundary field_;
  StartupResourceLoad resourceLoad_;
  StartupResourcePump resourcePump_;
  StartupAudioWait startupAudio_;
  FrameSuffix frameSuffix_;
  uint64_t budgetExitsThisField_ = 0;
  bool bootEntered_ = false;
};

} // namespace ctr
