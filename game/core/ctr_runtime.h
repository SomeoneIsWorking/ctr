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

  bool installOverride(Core &core, uint32_t address, std::string_view name, psx::cpu::NativeFunction function) const;
  bool removeOverride(Core &core, uint32_t address) const;
  psx::cpu::ExecutionResult dispatch(Core &core, uint32_t address) const;
  void dispatchToReturn(Core &core, uint32_t address, std::string_view owner) const;
  psx::cpu::ExecutionResult dispatchToContinuation(Core &core, uint32_t address, uint32_t continuation) const;
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
