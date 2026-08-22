#include "ctr_runtime.h"

#include "core.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {

CtrRuntime::CtrRuntime(Dispatch dispatch, uint32_t bootTarget) : dispatch_(dispatch), bootTarget_(bootTarget) {
  if (!dispatch_ || bootTarget_ == 0) {
    lucent::error("ctr-runtime",
                  "runtime requires a generated-code dispatch and a nonzero independently validated boot target");
    std::abort();
  }
}

uint32_t CtrRuntime::bootTarget() const {
  return bootTarget_;
}

void *CtrRuntime::createContext(Core &) {
  return nullptr;
}

void CtrRuntime::destroyContext(void *) {}

void CtrRuntime::registerOverrides(Game &) {
  // CTR has no Game or native override registry yet. The trace harness installs invocation-scoped
  // capture points in the generated registry after validating each requested address.
}

void CtrRuntime::bootInit(Core &core) {
  dispatch_(&core, bootTarget_);
}

} // namespace ctr
