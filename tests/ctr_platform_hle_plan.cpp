// The platform HLE plan CTR hands the framework: VSync(-1) answers from libetc's own vblank counter.

#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "native_ownership.h"
#include "platform_hle.h"

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

} // namespace

int main() {
  ctr::CtrRuntime runtime(ctr::native::kExecutableEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  game->platform_hle.initBuiltins();

  const OverrideFn vsync = game->platform_hle.lookup(ctr::native::kVSync);
  check(vsync != nullptr, "VSync is not bound");
  if (vsync == nullptr) {
    return 1;
  }
  game->core.mem_w32(ctr::native::kVSyncQueryCounter, 73u);
  game->core.r[4] = static_cast<std::uint32_t>(-1);
  game->core.r[2] = 0xDEADBEEFu;
  vsync(&game->core);
  check(game->core.r[2] == 73u, "VSync(-1) did not return libetc's vblank counter");
  return failures == 0 ? 0 : 1;
}
