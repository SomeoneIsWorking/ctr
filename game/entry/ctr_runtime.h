#pragma once

#include "execution_exit.h"
#include "game_runtime.h"
#include "guest_cd_stream_callback_layout.h"
#include "native_dispatch.h"
#include "native_ownership.h"
#include "scene_cut.h"
#include "widescreen_owner.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace ctr {

// Process-lifetime owner of CTR's framework-facing behavior; the title frame driver owns all dispatches after boot.
class CtrRuntime final : public GameRuntime {
public:
  explicit CtrRuntime(uint32_t bootTarget);

  uint32_t bootTarget() const;

  RenderCapabilities renderCapabilities() const override;
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  // CTR uses stock libcd with its own callbacks, so the framework stands in for the BIOS CD-ROM interrupt handler.
  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override;
  bool guestVramIsPicture(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  [[nodiscard]] SceneCut &sceneCut();

  // The base nullptr would resolve every plan to 4:3.
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  [[nodiscard]] CtrWidescreen &widescreen();

  // The field turn: not a guest call, its terminal exit is the typed frame boundary.
  psx::cpu::ExecutionResult dispatch(Core &core, uint32_t address) const;
  uint32_t callToContinuation(Core &core, uint32_t address, uint32_t returnPc, std::string_view owner) const;
  // The one nested hop whose terminal exit may be a frame boundary; the caller keeps the exit.
  psx::cpu::ExecutionResult dispatchHop(Core &core, uint32_t address, uint32_t returnPc) const;
  void callToReturn(Core &core, uint32_t address, std::string_view owner) const;
  void callOriginalToReturn(Core &core, uint32_t address, std::string_view owner) const;
  void propagateFrameBoundary(Core &core, const psx::cpu::ExecutionResult &result, std::string_view owner) const;

private:
  // KSEG0 executable extent of SCUS_944.26, in the physical addresses `GuestProgramImage` stores.
  static constexpr GuestProgramImage programImage_{.residentText = {0x00010000u, 0x0008D800u}};
  // The guest interrupt delivers CD data-ready: 0x8001C7FC walks [0x8008D708] 2 -> 3 -> 4 per audio sector,
  // and state zero waits on it at 0x8003C94C.
  static constexpr GuestCdStreamCallbackLayout cdStreamLayout_{
      .readyCallbackPointer = native::kCdReadyCallbackSlot,
      .owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
      .readyStatus = native::kCdReadyCompletionStatus,
      .stockReadRaisesCompletion = false,
  };
  const uint32_t bootTarget_;
  CtrWidescreen widescreen_{};
  SceneCut sceneCut_;
};

} // namespace ctr
