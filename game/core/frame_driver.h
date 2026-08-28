#pragma once

#include "dma_callback_owner.h"
#include "frame_callback_owner.h"
#include "game_runtime.h"
#include "presentation_owner.h"
#include "projection_owner.h"

#include <cstdint>

namespace ctr {

class CtrRuntime;

// Owns one finite CTR state-3 frame at a time. Generated code remains the behavior oracle on both
// sides of each extracted VSync callsite; title-local bridges resume at identity-gated re-entry
// points without ever invoking guest libetc VSync.
class CtrFrameDriver final : public FrameDriver {
public:
  explicit CtrFrameDriver(CtrRuntime &runtime);

  void stepFrame(Core &core, uint32_t frame) override;

  [[nodiscard]] uint32_t completedFrames() const;
  [[nodiscard]] const ProjectionOwner &projection() const;
  [[nodiscard]] const PresentationOwner &presentation() const;

private:
  enum class BootResourcePumpPhase : uint8_t {
    Inactive,
    ResourcePoll,
    CommitPoll,
  };

  static CtrFrameDriver *active_;

  static void skipFirstStartupVSync(Core *core);
  static void skipSecondStartupVSync(Core *core);
  static void waitForBootResourceWithoutVSync(Core *core);
  static void pumpBootResourceWithoutBusyWait(Core *core);
  static void serviceStartupAudioWithoutBusyWait(Core *core);
  static void continueStartupAudioLoop(Core *core);
  static void skipShutdownVSync(Core *core);
  static void observeVblankCallback(Core *core);
  static void finishFrameWithoutDebugVSync(Core *core);
  static void publishProjection(Core *core);

  void
  continueAfterVSync(Core &core, uint32_t superAddress, uint32_t expectedReturn, uint32_t continuation, uint32_t mode);
  void beginBootResourceWait(Core &core);
  void resumeBootResourceWait(Core &core);
  void beginBootResourcePump(Core &core);
  void resumeBootResourcePump(Core &core);
  void finishBootResourcePump(Core &core);
  void servicePendingInterrupts(Core &core);
  void resumeStartupAudioLoop(Core &core);
  void serviceStartupAudio(Core &core);
  void resumeStartupAudioWait(Core &core);
  void completeFrame(Core &core);
  void resumeFrameSuffix(Core &core);
  [[nodiscard]] bool frameSuffixIsWaiting(Core &core) const;
  void publishMeasuredProjection(Core &core);

  CtrRuntime &runtime_;
  DmaCallbackOwner dmaCallbacks_;
  FrameCallbackOwner frameCallbacks_;
  ProjectionOwner projection_;
  PresentationOwner presentation_;
  uint32_t completedFrames_ = 0;
  uint32_t bootResourceWaitFields_ = 0;
  uint32_t bootResourceWaitResume_ = 0;
  uint32_t startupAudioWaitResume_ = 0;
  bool frameSuffixPending_ = false;
  BootResourcePumpPhase bootResourcePumpPhase_ = BootResourcePumpPhase::Inactive;
  bool bootEntered_ = false;
};

} // namespace ctr
