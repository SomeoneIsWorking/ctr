#pragma once

#include "async_disc_owner.h"
#include "execution_exit.h"
#include "field_boundary.h"
#include "frame_callback_owner.h"
#include "frame_suffix.h"
#include "game_runtime.h"
#include "geometry_projection_owner.h"
#include "presentation_owner.h"
#include "projection_owner.h"
#include "spu_dma_callback_registration.h"
#include "startup_audio_wait.h"
#include "startup_resource_load.h"
#include "startup_resource_pump.h"
#include "widescreen_owner.h"

#include <cstdint>
#include <optional>

namespace ctr {

class CtrRuntime;

// Owns one CTR state-3 frame at a time and picks the continuation owed for the field it enters.
class CtrFrameDriver final : public FrameDriver {
public:
  explicit CtrFrameDriver(CtrRuntime &runtime);

  void stepFrame(Core &core, uint32_t frame) override;

  [[nodiscard]] uint32_t completedFrames() const;
  [[nodiscard]] uint64_t budgetExitsForLastField() const;
  [[nodiscard]] const ProjectionOwner &projection() const;
  [[nodiscard]] const CtrWidescreen &widescreen() const;
  [[nodiscard]] const PresentationOwner &presentation() const;
  [[nodiscard]] DiscReadOwner &discReadOwner();

private:
  // Guest overrides are bare function pointers, so each callsite needs its own entry point.
  static void onStartupGpuVSync(Core *core);
  static void onStartupDisplayVSync(Core *core);
  static void onBootResourceWait(Core *core);
  static void onBootResourcePump(Core *core);
  static void onStartupAudioService(Core *core);
  static void onShutdownVSync(Core *core);
  static void onVblankCallback(Core *core);
  static void onFrameTiming(Core *core);
  static void onProjectionProducer(Core *core);
  static void onDmaCallbackRegistration(Core *core);

  // `dispatchField` is the only place a budget exit may be resumed as the same field.
  void resumeOwedContinuation(Core &core);
  void refuseUnfinishedField(Core &core, uint32_t frame, const std::optional<psx::cpu::ExecutionResult> &execution);
  [[nodiscard]] psx::cpu::ExecutionResult dispatchField(Core &core, uint32_t frame, uint32_t entry);
  void publishMeasuredProjection(Core &core);
  [[nodiscard]] static ProjectionOwner::Source classifyProjectionSource(uint32_t returnAddress);

  CtrRuntime &runtime_;
  DiscReadOwner discReadOwner_;
  FrameCallbackOwner frameCallbacks_;
  ProjectionOwner projection_;
  CtrGeometryProjectionOwner geometryProjection_;
  CtrWidescreen widescreen_;
  PresentationOwner presentation_;
  FieldBoundary field_;
  StartupResourceLoad resourceLoad_;
  StartupResourcePump resourcePump_;
  StartupAudioWait startupAudio_;
  SpuDmaCallbackRegistration spuDmaRegistration_;
  FrameSuffix frameSuffix_;
  uint64_t budgetExitsThisField_ = 0;
  bool bootEntered_ = false;
};

// A missing or foreign driver is refused.
[[nodiscard]] CtrFrameDriver &ctrFrameDriver(Core &core);

} // namespace ctr
