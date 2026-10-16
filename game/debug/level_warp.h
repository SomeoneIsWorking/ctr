#pragma once

#include <cstdint>
#include <optional>
#include <string>

class Core;

namespace ctr {

// The `warp <level-hex>` control request: armed between fields while a race runs, applied at the next field start
// through the guest's own level-load request, which main-loop state 3 consumes through its loader.
class LevelWarp {
public:
  // Parses one command line; the returned text answers the client.
  std::string arm(const char *line, Core &core);
  // Raises the armed request once the race still allows one; returns the level requested.
  std::optional<uint32_t> apply(Core &core);

private:
  // A race track is running and the guest has no load of its own pending.
  static bool legal(Core &core);

  std::optional<uint32_t> armed_;
};

} // namespace ctr
