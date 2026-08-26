#include "ctr_runtime.h"

#include "core.h"
#include "projection_hle_plan.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {

const GuestProgramImage CtrRuntime::programImage_{
    // GuestProgramImage stores physical addresses; CTR-01 measured the corresponding KSEG0
    // executable extent as [0x80010000,0x8008D800).
    .residentText = {0x00010000u, 0x0008D800u},
};

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

RenderCapabilities CtrRuntime::renderCapabilities() const {
  return RenderCapabilities::interpolatedNative();
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

const GuestProgramImage *CtrRuntime::guestProgramImage() const {
  return &programImage_;
}

const PlatformHlePlan *CtrRuntime::platformHlePlan() const {
  return &projectionHlePlan();
}

bool CtrRuntime::guestVramIsPicture(const Game &) const {
  // CTR's bounded product boot neither builds nor presents a guest frame, so claiming guest VRAM as
  // picture content here would invent rendered-frame ownership.
  return false;
}

} // namespace ctr
