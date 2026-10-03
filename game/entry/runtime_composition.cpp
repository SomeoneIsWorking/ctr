#include "runtime_composition.h"

#include "game.h"
#include "render_mode.h"

namespace ctr {

void installRuntimeOwners(Game &game) {
  // Direct runtimes have no legacy GameConfig, so give the native disc backend CTR's own media key
  // at the subsystem boundary. disc_open retains its generic env/.env/drop-in fallbacks.
  game.disc.env_key = "PSXPORT_CTR_DISC";
  game.core.runtime->registerOverrides(game);
  render_path_install(&game.core);
}

} // namespace ctr
