#include "debug/level_warp.h"

#include "core.h"
#include "execution/guest_turns.h"
#include "title/native_ownership.h"
#include "video/scene_cut.h"

#include <charconv>
#include <lucent/log.h>
#include <string_view>

namespace ctr {
namespace {

// The hex level id after the command word; an optional 0x prefix is accepted.
std::optional<uint32_t> parseLevel(std::string_view line) {
  const std::size_t start = line.find_first_not_of(' ', line.find(' '));
  if (start == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view digits = line.substr(start);
  if (digits.starts_with("0x") || digits.starts_with("0X")) {
    digits.remove_prefix(2);
  }
  uint32_t level = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), level, 16);
  if (error != std::errc{} || end == digits.data()) {
    return std::nullopt;
  }
  return level;
}

} // namespace

bool LevelWarp::legal(Core &core) {
  const SceneIdentity scene = SceneCut::read(core);
  return scene.mainState == native::kMainStateRunning && !scene.loading &&
         scene.levelId <= native::kLastRaceTrackLevel &&
         core.mem_r32(core.r[28] + native::kLoadRequestGpOffset) == native::kNoLoadRequest;
}

std::string LevelWarp::arm(const char *line, Core &core) {
  const std::optional<uint32_t> parsed = parseLevel(line);
  if (!parsed) {
    return "usage: warp <level-hex>";
  }
  if (*parsed > native::kLastRaceTrackLevel) {
    return lucent::format(
        "refused: level 0x{:X} is past the last race track 0x{:X}", *parsed, native::kLastRaceTrackLevel);
  }
  if (!legal(core)) {
    return "refused: a warp is legal only during a race with no load pending";
  }
  armed_ = parsed;
  return lucent::format("ok: warp armed for level 0x{:X}", *parsed);
}

std::optional<uint32_t> LevelWarp::apply(Core &core) {
  if (!armed_ || !legal(core)) {
    return std::nullopt;
  }
  const uint32_t level = *armed_;
  armed_.reset();
  // The field resumes the main loop with the registers it stopped with.
  const R3000 resume = core;
  core.r[psx::cpu::kA0] = level;
  core.r[psx::cpu::kRa] = native::kFrameLoopResume;
  guest::callToReturn(core, native::kRequestLevelLoad, "CTR level warp");
  static_cast<R3000 &>(core) = resume;
  lucent::info("ctr-warp", "level 0x{:X} requested", level);
  return level;
}

} // namespace ctr
