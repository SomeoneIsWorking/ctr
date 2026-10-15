#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

#include <cstdint>
#include <memory>

class Core;
class Game;

namespace ctr {

// CTR (SCUS_944.26) on Record: the stock libetc/libgte/libcd bindings at this executable's addresses, the title's
// own libgpu-timeout and CD leaves, and the frame driver that owns every dispatch after boot.
class Runtime final : public psx::game::GameRuntime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;

  // A change of main-loop state, level or loading bit between two committed fields.
  bool sealedFrameIsCut(Core &core) const override;
  // The base nullptr would resolve every plan to 4:3.
  const GuestWidescreenProjection *guestWidescreenProjection() const override;

  [[nodiscard]] const psx::cd::StockLibcd &libcd() const {
    return libcd_;
  }

  // The runtime of the title running on `core`; another title's core stops the run.
  static const Runtime &from(const Core &core);

private:
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgte libgte_;
  // The guest interrupt delivers CD data-ready: 0x8001C7FC walks [0x8008D708] 2 -> 3 -> 4 per audio sector,
  // and state zero waits on it at 0x8003C94C.
  psx::cd::GuestInterruptLibcd libcd_;
  GuestWidescreenProjection widescreen_;
};

} // namespace ctr
