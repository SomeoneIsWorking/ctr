#include "ctr_runtime.h"

#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "platform_hle.h"

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
  const RenderCapabilities renderCapabilities = runtime.renderCapabilities();
  if (renderCapabilities.defaultPath != RenderPath::Native || !renderCapabilities.nativeRenderPath ||
      !renderCapabilities.temporalInterpolation || renderCapabilities.playerPathCount() != 2) {
    std::fprintf(stderr, "CtrRuntime did not declare its native widescreen/interpolation target\n");
    return 1;
  }
  const PlatformHlePlan *platformPlan = runtime.platformHlePlan();
  if (platformPlan == nullptr || platformPlan->setGeomScreen != 0x8007781Cu ||
      platformPlan->setGeomOffset != 0x8007782Cu || platformPlan->bindingCount != 0 ||
      platformPlan->windowLo[0] != 0x8007781Cu || platformPlan->windowHi[0] != 0x80077844u ||
      platformPlan->windowLo[1] != 0 || platformPlan->windowHi[1] != 0) {
    std::fprintf(stderr, "CtrRuntime lost its identity-gated libgte projection plan\n");
    return 1;
  }
  auto game = std::make_unique<Game>();
  game->platform_hle.initBuiltins();
  OverrideFn setScreen = game->platform_hle.lookup(platformPlan->setGeomScreen);
  OverrideFn setOffset = game->platform_hle.lookup(platformPlan->setGeomOffset);
  if (setScreen == nullptr || setOffset == nullptr || game->platform_hle.lookup(platformPlan->windowHi[0]) != nullptr) {
    std::fprintf(stderr, "CtrRuntime projection leaves did not respect the exact retail window\n");
    return 1;
  }
  Core &projectionCore = game->core;
  gte_bind(&projectionCore);
  projectionCore.r[4] = 320u;
  setScreen(&projectionCore);
  projectionCore.r[4] = 256u;
  projectionCore.r[5] = 120u;
  setOffset(&projectionCore);
  if (!projectionCore.rsub.projParams.geomValid() || projectionCore.rsub.projParams.geomH() != 320.0f ||
      projectionCore.rsub.projParams.geomOfx() != 256.0f || projectionCore.rsub.projParams.geomOfy() != 120.0f) {
    std::fprintf(stderr, "CtrRuntime projection HLE did not record the measured boot projection\n");
    return 1;
  }
  if (game_guest_vram_is_picture(*game)) {
    std::fprintf(stderr, "CtrRuntime claimed guest VRAM picture ownership without a rendered frame\n");
    return 1;
  }

  runtime.bootInit(*core);
  if (g_dispatchedCore != core.get() || g_dispatchedAddress != kValidatedEntry) {
    std::fprintf(stderr, "CtrRuntime did not dispatch the configured trace boundary\n");
    return 1;
  }

  std::puts("CtrRuntime: direct derived install, native/interpolated render target, measured resident "
            "image/projection leaves, no legacy views/picture claim, configured trace dispatch");
  return 0;
}
