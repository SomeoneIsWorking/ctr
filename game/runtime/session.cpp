#include "runtime/session.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {

Session &session(Core &core) {
  auto *const owned = core.game == nullptr ? nullptr : dynamic_cast<Session *>(core.game->sessionState.get());
  if (owned == nullptr) {
    lucent::error("ctr-core", "a CTR owner reached a Core whose Game has no CTR session");
    std::abort();
  }
  return *owned;
}

} // namespace ctr
