// The libraries Runtime::registerOverrides binds at SCUS_944.26's addresses, reached through the HLE table.

#include "core.h"
#include "game.h"
#include "platform_hle.h"
#include "runtime/runtime.h"
#include "title/native_ownership.h"

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

OverrideFn bound(Game &game, std::uint32_t address, const char *name) {
  const OverrideFn function = game.platform_hle.lookup(address);
  if (function == nullptr) {
    std::printf("FAIL: %s is not bound\n", name);
    ++failures;
  }
  return function;
}

} // namespace

int main() {
  ctr::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  runtime.registerOverrides(*game);
  Core &core = game->core;

  check(bound(*game, ctr::native::kSetGeomOffset, "SetGeomOffset") != nullptr, "libgte offset");
  check(bound(*game, ctr::native::kSetGeomScreen, "SetGeomScreen") != nullptr, "libgte screen");
  check(bound(*game, ctr::native::kCdRead, "CdRead") != nullptr, "CdRead");
  check(bound(*game, ctr::native::kCdReadSync, "CdReadSync") != nullptr, "CdReadSync");
  check(game->platform_hle.vsync() != nullptr && game->platform_hle.vsyncAddress() == ctr::native::kVSync,
        "VSync is not the native frame boundary");
  check(game->cd.stockLibrary() == &runtime.libcd(), "the Game's libcd is not CTR's");
  check(runtime.libcd().deliversOnInterrupt(), "CD data-ready is not left to the guest interrupt");

  if (const OverrideFn vsync = bound(*game, ctr::native::kVSync, "VSync")) {
    core.mem_w32(ctr::native::kVSyncQueryCounter, 73u);
    core.r[4] = static_cast<std::uint32_t>(-1);
    core.r[2] = 0xDEADBEEFu;
    vsync(&core);
    check(core.r[2] == 73u, "VSync(-1) did not return libetc's vblank counter");
  }

  if (const OverrideFn arm = bound(*game, ctr::native::kGpuTimeoutArm, "libgpu timeout arm")) {
    arm(&core);
    const std::uint32_t deadline = core.mem_r32(ctr::native::kGpuTimeoutDeadline);
    check(deadline > game->timing.vblank && core.r[2] == deadline, "the timeout arm published no field deadline");
  }
  if (const OverrideFn timeoutCheck = bound(*game, ctr::native::kGpuTimeoutCheck, "libgpu timeout check")) {
    timeoutCheck(&core);
    check(core.r[2] == 0u && core.mem_r32(ctr::native::kGpuTimeoutPollCount) == 1u,
          "an unexpired timeout check did not count its poll");
  }

  constexpr std::uint32_t kResult = 0x80100000u;
  if (const OverrideFn sync = bound(*game, ctr::native::kCdSync, "CdSync")) {
    core.mem_w32(kResult, 0xFFFFFFFFu);
    core.mem_w32(kResult + 4u, 0xFFFFFFFFu);
    core.r[5] = kResult;
    sync(&core);
    check(core.r[2] == ctr::native::kCdlComplete, "CdSync did not report CdlComplete");
    check(core.mem_r32(kResult) == 0u && core.mem_r32(kResult + 4u) == 0u, "CdSync left result bytes");
  }
  if (const OverrideFn command = bound(*game, ctr::native::kCdCommand, "CD_cw")) {
    core.r[4] = 0x09u; // Pause
    core.r[5] = 0u;
    core.r[6] = 0u;
    command(&core);
    check(core.r[2] == 0u, "CD_cw 0x8007BC38 did not report success as zero");
  }

  std::printf("CTR platform bindings: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
