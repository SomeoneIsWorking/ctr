#pragma once

class Game;

namespace ctr {

// The one production and test composition path for title overrides and hardware-sync ownership.
void installRuntimeOwners(Game &game);

} // namespace ctr
