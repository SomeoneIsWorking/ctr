#include "presentation_owner.h"

#include "core.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

void PresentationOwner::finishField(Core &core) {
  if (!core.game) {
    lucent::error("ctr-presentation", "frame fence requires a bound Game");
    std::abort();
  }

  const uint64_t before = core.game->presentation.fence();
  if (hasPresentableCapture(core)) {
    // The native loop owns one host display field per step. No temporal decorator is admitted until
    // CTR has native producers and measured previous/current transform sources.
    core.game->presentation.commit(&core, 1);
    ++presentedFences_;
  } else {
    core.game->presentation.commitUnpresented(&core);
  }
  if (core.game->presentation.fence() != before + 1u) {
    lucent::error("ctr-presentation", "framework did not advance exactly one frame fence");
    std::abort();
  }
  ++completedFences_;
}

bool PresentationOwner::hasPresentableCapture(const Core &core) const {
  return core.game && core.game->presentation.capturedCount() > 0;
}

uint64_t PresentationOwner::completedFences() const {
  return completedFences_;
}

uint64_t PresentationOwner::presentedFences() const {
  return presentedFences_;
}

} // namespace ctr
