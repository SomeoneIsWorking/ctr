#include "runtime_composition.h"

#include "game.h"

namespace ctr {

void installRuntimeOwners(Game &game) {
  // Direct runtimes have no GameConfig, so the disc backend gets CTR's media key here.
  game.disc.env_key = "PSXPORT_CTR_DISC";
}

} // namespace ctr
