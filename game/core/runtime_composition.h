#pragma once

class Game;

namespace ctr {

// The single production/test composition path for title overrides and hardware-sync ownership.
void installRuntimeOwners(Game &game);

} // namespace ctr
