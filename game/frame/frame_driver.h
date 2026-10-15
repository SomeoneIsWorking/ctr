#pragma once

#include "boot/startup_audio_wait.h"
#include "boot/startup_resource_load.h"
#include "boot/startup_resource_pump.h"
#include "disc/async_disc_owner.h"
#include "execution/spu_dma_callback_registration.h"
#include "execution_exit.h"
#include "frame/field_boundary.h"
#include "frame/frame_callback_owner.h"
#include "frame/frame_suffix.h"
#include "game_runtime.h"
#include "video/geometry_projection_owner.h"
#include "video/presentation_owner.h"
#include "video/projection_owner.h"
#include "video/widescreen_owner.h"

#include <cstdint>
#include <optional>

class Core;
class Game;

namespace ctr {

// Owns one CTR state-3 frame at a time and picks the continuation owed for the field it enters. The first field
// enters the executable at the PC the admitted header set.
class FrameDriver final : public psx::game::FrameDriver {
public:
  explicit FrameDriver(Game &game);

  void stepFrame(Core &core, uint32_t frame) override;

  [[nodiscard]] uint32_t completedFrames() const;
  [[nodiscard]] uint64_t budgetExitsForLastField() const;
  [[nodiscard]] const ProjectionOwner &projection() const;
  [[nodiscard]] const WidescreenOwner &widescreen() const;
  [[nodiscard]] const PresentationOwner &presentation() const;

  // A missing or foreign driver is refused.
  static FrameDriver &from(Core &core);

private:
  // Guest overrides are bare function pointers, so each callsite needs its own entry point.
  static void onStartupGpuVSync(Core *core);
  static void onStartupDisplayVSync(Core *core);
  static void onBootResourceWait(Core *core);
  static void onBootResourcePump(Core *core);
  static void onStartupAudioService(Core *core);
  static void onShutdownVSync(Core *core);
  static void onFrameTiming(Core *core);
  static void onProjectionProducer(Core *core);
  static void onDmaCallbackRegistration(Core *core);

  // `dispatchField` is the only place a budget exit may be resumed as the same field.
  void resumeOwedContinuation(Core &core);
  void refuseUnfinishedField(Core &core, uint32_t frame, const std::optional<psx::cpu::ExecutionResult> &execution);
  [[nodiscard]] psx::cpu::ExecutionResult dispatchField(Core &core, uint32_t entry);
  void publishMeasuredProjection(Core &core);
  [[nodiscard]] static ProjectionOwner::Source classifyProjectionSource(uint32_t returnAddress);

  DiscReadOwner &discReadOwner_;
  FrameCallbackOwner frameCallbacks_;
  ProjectionOwner projection_;
  GeometryProjectionOwner geometryProjection_;
  WidescreenOwner widescreen_;
  PresentationOwner presentation_;
  FieldBoundary field_;
  StartupResourceLoad resourceLoad_;
  StartupResourcePump resourcePump_;
  StartupAudioWait startupAudio_;
  SpuDmaCallbackRegistration spuDmaRegistration_;
  FrameSuffix frameSuffix_;
  uint64_t budgetExitsThisField_ = 0;
  bool bootEntered_ = false;
  // The host loop's number for the previous field; the base is the loop's, consecutiveness is ours.
  std::optional<uint32_t> lastFrame_;
};

} // namespace ctr
