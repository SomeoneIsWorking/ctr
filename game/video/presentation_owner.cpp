#include "presentation_owner.h"

#include "core.h"
#include "game.h"
#include "scene_cut.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

PresentationOwner::PresentationOwner(SceneCut &sceneCut) : sceneCut_(sceneCut) {}

void PresentationOwner::finishField(Core &core) {
  if (!core.game) {
    lucent::error("ctr-presentation", "frame fence requires a bound Game");
    std::abort();
  }

  sceneCut_.observe(SceneCut::read(core));
  const uint64_t before = core.game->presentation.fence();
  // The record path seals the device's own GP0 record; no temporal decorator until CTR has native producers.
  core.game->presentation.commit(&core, 1);
  if (core.game->presentation.fence() != before + 1u) {
    lucent::error("ctr-presentation", "framework did not advance exactly one frame fence");
    std::abort();
  }
  ++completedFences_;
}

uint64_t PresentationOwner::completedFences() const {
  return completedFences_;
}

} // namespace ctr
