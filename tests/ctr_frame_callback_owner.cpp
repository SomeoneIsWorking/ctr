// FrameCallbackOwner: the field seam delivers a pending vblank to the guest's own interrupt chain, once per edge.

#include "core.h"
#include "ctr_runtime.h"
#include "frame_callback_owner.h"
#include "game.h"
#include "image_identity.h"
#include "native_dispatch.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

constexpr std::uint32_t kEntry = 0x80010000u;
constexpr std::uint32_t kHandler = 0x80020000u;
constexpr std::uint32_t kElement = 0x80100010u;
constexpr std::uint32_t kStack = 0x801FFF00u;

int handlerCalls = 0;

// Stands in for the libetc ISR chain: counts the call and acknowledges the vblank bit.
void isr(Core *core) {
  ++handlerCalls;
  core->mem_w32(0x1F801070u, 0x7FEu);
}

void raiseVblank(Game &game) {
  game.hle.i_stat |= 1u;
  game.core.pending_work |= Core::PW_IRQ;
}

} // namespace

int main() {
  ctr::CtrRuntime runtime(kEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.r[29] = kStack;
  const auto image = core.imageCatalog().activate("ctr-frame-callback-owner", {0x00010000u, 0x0008d800u}, 1u);
  check(core.nativeDispatcher().install({{image, kHandler}, "vblank isr", isr}), "could not install the ISR stand-in");
  core.mem_w32(kElement + 4u, kHandler);
  core.mem_w32(kElement + 8u, 0u);
  game->hle.irqEnq(2, kElement);
  game->hle.i_mask = 1u;

  ctr::FrameCallbackOwner owner;
  raiseVblank(*game);
  owner.deliverField(core, runtime);
  check(handlerCalls == 1, "a pending vblank did not reach the guest's interrupt chain at the field seam");
  owner.deliverField(core, runtime);
  check(handlerCalls == 1, "an acknowledged vblank was delivered again");
  raiseVblank(*game);
  owner.deliverField(core, runtime);
  check(handlerCalls == 2, "the next field's vblank was not delivered");
  return failures == 0 ? 0 : 1;
}
