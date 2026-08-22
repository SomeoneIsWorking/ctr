#include "ctr_runtime.h"

#include "core.h"
#include "game_runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <type_traits>

namespace {

Core *g_dispatchedCore = nullptr;
uint32_t g_dispatchedAddress = 0;

void captureDispatch(Core *core, uint32_t address) {
  g_dispatchedCore = core;
  g_dispatchedAddress = address;
}

} // namespace

int main() {
  static_assert(std::is_base_of_v<GameRuntime, ctr::CtrRuntime>);

  constexpr uint32_t kValidatedEntry = 0x8007793Cu;
  ctr::CtrRuntime runtime(captureDispatch, kValidatedEntry);
  psxport_install_game(runtime);

  auto core = std::make_unique<Core>();
  if (psxport_game_runtime() != &runtime || core->runtime != &runtime) {
    std::fprintf(stderr, "CtrRuntime was not installed as Core's derived runtime\n");
    return 1;
  }
  if (runtime.legacyConfigForMigration() != nullptr || runtime.legacyHooksForMigration() != nullptr ||
      core->cfg != nullptr || core->hooks != nullptr) {
    std::fprintf(stderr, "CtrRuntime exposed a legacy GameConfig/GameHooks view\n");
    return 1;
  }
  if (core->gameCtx != nullptr || runtime.bootTarget() != kValidatedEntry) {
    std::fprintf(stderr, "CtrRuntime invented context state or lost its validated target\n");
    return 1;
  }

  runtime.bootInit(*core);
  if (g_dispatchedCore != core.get() || g_dispatchedAddress != kValidatedEntry) {
    std::fprintf(stderr, "CtrRuntime did not dispatch the configured trace boundary\n");
    return 1;
  }

  std::puts("CtrRuntime: direct derived install, no legacy views, configured trace dispatch");
  return 0;
}
