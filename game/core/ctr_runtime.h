#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <memory>

namespace ctr {

// Process-lifetime owner of CTR's framework-facing behavior. It derives the real runtime seam
// directly: no legacy config or callback bag exists to adapt. The measured executable entry is
// configured once before Core construction; the title frame driver owns all later dispatches.
class CtrRuntime final : public GameRuntime {
public:
  using Dispatch = void (*)(Core *core, uint32_t address);
  using RecompiledOverride = void (*)(Core *core);
  using OverrideSetter = void (*)(uint32_t address, RecompiledOverride overrideFunction);
  using SuperDispatch = void (*)(Core *core, uint32_t address);

  CtrRuntime(Dispatch dispatch,
             uint32_t bootTarget,
             OverrideSetter overrideSetter = nullptr,
             SuperDispatch superDispatch = nullptr);

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

  void setRecompiledOverride(uint32_t address, RecompiledOverride overrideFunction) const;
  void dispatch(Core &core, uint32_t address) const;
  void runRecompiledSuper(Core &core, uint32_t address) const;

private:
  static const GuestProgramImage programImage_;
  Dispatch dispatch_;
  OverrideSetter overrideSetter_;
  SuperDispatch superDispatch_;
  const uint32_t bootTarget_;
};

} // namespace ctr
