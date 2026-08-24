#include "ctr_runtime.h"

#include "core.h"
#include "game.h"
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
  const GuestProgramImage *programImage = runtime.guestProgramImage();
  if (programImage == nullptr || core->guestProgramImage != programImage ||
      programImage->residentText.begin != 0x00010000u || programImage->residentText.end != 0x0008D800u ||
      !programImage->residentText.containsPhysical(0x80010000u) ||
      !programImage->residentText.containsPhysical(0x8008D7FFu) ||
      programImage->residentText.containsPhysical(0x8008D800u)) {
    std::fprintf(stderr, "CtrRuntime did not install the measured half-open resident program range\n");
    return 1;
  }
  if (core->gameCtx != nullptr || runtime.bootTarget() != kValidatedEntry) {
    std::fprintf(stderr, "CtrRuntime invented context state or lost its validated target\n");
    return 1;
  }
  auto game = std::make_unique<Game>();
  if (game_guest_vram_is_picture(*game)) {
    std::fprintf(stderr, "CtrRuntime claimed guest VRAM picture ownership without a rendered frame\n");
    return 1;
  }

  runtime.bootInit(*core);
  if (g_dispatchedCore != core.get() || g_dispatchedAddress != kValidatedEntry) {
    std::fprintf(stderr, "CtrRuntime did not dispatch the configured trace boundary\n");
    return 1;
  }

  std::puts("CtrRuntime: direct derived install, measured resident image, no legacy views/picture claim, "
            "configured trace dispatch");
  return 0;
}
