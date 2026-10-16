// LevelWarp through the shipping owner and the runtime's control surface, over a stub level-load request.

#include "core.h"
#include "debug/level_warp.h"
#include "game.h"
#include "runtime/runtime.h"
#include "title/native_ownership.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

bool contains(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}

constexpr std::uint32_t kStack = 0x00120000u;
constexpr std::uint32_t kGlobalBlock = 0x00130000u;
constexpr std::uint32_t kTracker = 0x00096000u;
constexpr std::uint32_t kLevelGpOffset = 0x190u;
constexpr std::uint32_t kCrashCove = 3u;

// The retail request's effect on the two words state 3 reads, and a clobbered temporary.
void installRequestStub(Core &core) {
  const std::uint32_t at = ctr::native::kRequestLevelLoad;
  core.mem_w32(at, 0x2408FFFCu);       // addiu t0, zero, -4
  core.mem_w32(at + 4u, 0xAF88018Cu);  // sw t0, 0x18C(gp)
  core.mem_w32(at + 8u, 0xAF840190u);  // sw a0, 0x190(gp)
  core.mem_w32(at + 12u, 0x03E00008u); // jr ra
  core.mem_w32(at + 16u, 0u);
}

void enterRace(Core &core) {
  core.mem_w32(kGlobalBlock + ctr::native::kGameStateGpOffset, kTracker);
  core.mem_w32(kGlobalBlock + ctr::native::kMainStateGpOffset, ctr::native::kMainStateRunning);
  core.mem_w32(kGlobalBlock + ctr::native::kLoadRequestGpOffset, ctr::native::kNoLoadRequest);
  core.mem_w32(kTracker + ctr::native::kTrackerLevelIdOffset, kCrashCove);
  core.mem_w32(kTracker + ctr::native::kTrackerGameModeOffset, 0u);
}

std::string command(Game &game, const char *line) {
  std::FILE *out = std::tmpfile();
  const bool handled = game.runtime.titleCommand(game.core, {.command = "warp", .line = line}, out);
  char reply[160] = {};
  const bool read = std::fseek(out, 0, SEEK_SET) == 0 && std::fread(reply, 1, sizeof reply - 1, out) > 0;
  std::fclose(out);
  return handled && read ? std::string(reply) : std::string();
}

} // namespace

int main() {
  ctr::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  core.imageCatalog().activate("ctr-level-warp", ctr::native::kResidentText, 1u);
  core.r[28] = kGlobalBlock;
  core.r[29] = kStack;
  installRequestStub(core);

  ctr::LevelWarp warp;
  check(contains(warp.arm("warp", core), "usage"), "a warp with no level was not answered with its usage");
  check(contains(warp.arm("warp 12", core), "refused"), "a level past the last race track was armed");
  check(contains(warp.arm("warp 6", core), "refused"), "a warp before any race was armed");
  check(!warp.apply(core), "an unarmed warp applied");

  enterRace(core);
  core.mem_w32(kTracker + ctr::native::kTrackerLevelIdOffset, 0x27u);
  check(contains(warp.arm("warp 6", core), "refused"), "a warp from the main menu was armed");
  enterRace(core);
  core.mem_w32(kGlobalBlock + ctr::native::kLoadRequestGpOffset, 0xFFFFFFFCu);
  check(contains(warp.arm("warp 6", core), "refused"), "a warp over the guest's own pending load was armed");

  enterRace(core);
  check(contains(warp.arm("warp 0x6", core), "ok"), "a race-track warp during a race was refused");
  core.mem_w32(kTracker + ctr::native::kTrackerGameModeOffset, ctr::native::kGameModeLoading);
  check(!warp.apply(core), "an armed warp applied while the guest was loading");
  core.mem_w32(kTracker + ctr::native::kTrackerGameModeOffset, 0u);

  core.r[4] = 0x1234u;
  core.r[8] = 0x55u;
  core.r[31] = 0x80012345u;
  core.pc = ctr::native::kFrameLoopResume;
  check(warp.apply(core) == std::optional<std::uint32_t>(6u), "an armed warp did not apply at the next field");
  check(core.mem_r32(kGlobalBlock + kLevelGpOffset) == 6u, "the guest request did not receive the level");
  check(core.mem_r32(kGlobalBlock + ctr::native::kLoadRequestGpOffset) == 0xFFFFFFFCu,
        "the guest's own request did not raise its load");
  check(core.r[4] == 0x1234u && core.r[8] == 0x55u && core.r[31] == 0x80012345u &&
            core.pc == ctr::native::kFrameLoopResume && core.r[28] == kGlobalBlock && core.r[29] == kStack,
        "the field did not resume with the registers it stopped with");
  check(!warp.apply(core), "a warp applied twice");

  enterRace(core);
  check(contains(command(*game, "warp 3"), "ok"), "the runtime did not arm `warp` through its frame driver");

  std::printf("level warp: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
