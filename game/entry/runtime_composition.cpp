#include "runtime_composition.h"

#include "game.h"
#include "render_mode.h"

namespace ctr {

void installRuntimeOwners(Game &game) {
  // Direct runtimes have no GameConfig, so the disc backend gets CTR's media key here.
  game.disc.env_key = "PSXPORT_CTR_DISC";
  render_path_install(&game.core);
}

} // namespace ctr
