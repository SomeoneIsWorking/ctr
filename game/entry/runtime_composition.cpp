#include "runtime_composition.h"

#include "machine.h"
#include "title_identity.h"

namespace ctr {

void installRuntimeOwners(Game &game) {
  psx::Machine{game}.insertDisc(kTitleIdentity);
}

} // namespace ctr
