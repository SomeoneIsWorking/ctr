#pragma once

#include "execution_exit.h"
#include "game_runtime.h"
#include "native_dispatch.h"
#include "widescreen_owner.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace ctr {

// Process-lifetime owner of CTR's framework-facing behavior. It derives the real runtime seam
// directly: no legacy config or callback bag exists to adapt. The measured executable entry is
// configured once before Core construction; the title frame driver owns all later dispatches.
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
  bool guestVramIsPicture(const Game &game) const override;

  // CTR's own answer to the framework's widescreen question, and the only thing standing between the
  // configured aspect and a plan: returning the base nullptr resolves every plan to 4:3 whatever the
  // settings file says.
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  [[nodiscard]] CtrWidescreen &widescreen();

  // The field turn itself: run guest code until it is not still this field's finite quantity. It is
  // NOT a guest call — it has no return boundary and its terminal exit is the typed frame boundary —
  // so it does not enter `psx::cpu::ResumableGuestCall`.
  psx::cpu::ExecutionResult dispatch(Core &core, uint32_t address) const;
  // One guest call bounded by `returnPc`, resumable across host turns, capped by `turnCap`.
  uint32_t callToContinuation(Core &core, uint32_t address, uint32_t returnPc, std::string_view owner) const;
  // The ONE nested hop whose terminal exit may be a typed frame boundary rather than a return, so it
  // keeps the exit in the caller's hands instead of being wrapped in a call-to-return (which refuses
  // anything but a guest return). Everything else on that path is a shared call.
  psx::cpu::ExecutionResult dispatchHop(Core &core, uint32_t address, uint32_t returnPc) const;
  void callToReturn(Core &core, uint32_t address, std::string_view owner) const;
  void callOriginalToReturn(Core &core, uint32_t address, std::string_view owner) const;
  void propagateFrameBoundary(Core &core, const psx::cpu::ExecutionResult &result, std::string_view owner) const;

private:
  // The measured KSEG0 executable extent of SCUS_944.26, in the physical addresses
  // `GuestProgramImage` stores.
  static constexpr GuestProgramImage programImage_{.residentText = {0x00010000u, 0x0008D800u}};
  const uint32_t bootTarget_;
  CtrWidescreen widescreen_{};
};

} // namespace ctr
