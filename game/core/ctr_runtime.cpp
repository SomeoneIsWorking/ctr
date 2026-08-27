#include "ctr_runtime.h"

#include "core.h"
#include "frame_driver.h"
#include "platform_hle_plan.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {

const GuestProgramImage CtrRuntime::programImage_{
    // GuestProgramImage stores physical addresses; CTR-01 measured the corresponding KSEG0
    // executable extent as [0x80010000,0x8008D800).
    .residentText = {0x00010000u, 0x0008D800u},
};

CtrRuntime::CtrRuntime(Dispatch dispatch,
                       uint32_t bootTarget,
                       OverrideSetter overrideSetter,
                       SuperDispatch superDispatch)
    : dispatch_(dispatch), overrideSetter_(overrideSetter), superDispatch_(superDispatch), bootTarget_(bootTarget) {
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
  // CTR has a measured projection publication owner, but no game-state primitive producer or
  // temporal transform source yet. Do not expose Native/FPS60 controls before those products exist.
  return {
      .defaultPath = RenderPath::Gte,
      .nativeRenderPath = false,
      .temporalInterpolation = false,
  };
}

void *CtrRuntime::createContext(Core &) {
  return nullptr;
}

void CtrRuntime::destroyContext(void *) {}

void CtrRuntime::registerOverrides(Game &) {
  // CTR's generated overrides are frame-scoped by CtrFrameDriver so no title hook can outlive the
  // finite host-owned iteration which installed it.
}

void CtrRuntime::bootInit(Core &core) {
  dispatch_(&core, bootTarget_);
}

std::unique_ptr<FrameDriver> CtrRuntime::createFrameDriver(Game &) {
  return std::make_unique<CtrFrameDriver>(*this);
}

const GuestProgramImage *CtrRuntime::guestProgramImage() const {
  return &programImage_;
}

const PlatformHlePlan *CtrRuntime::platformHlePlan() const {
  return &ctr::platformHlePlan();
}

bool CtrRuntime::guestVramIsPicture(const Game &) const {
  // The title driver preserves the generated frame order but does not present guest VRAM or own a
  // native primitive renderer. Claiming picture content would invent a presentation path.
  return false;
}

void CtrRuntime::setRecompiledOverride(uint32_t address, RecompiledOverride overrideFunction) const {
  if (!overrideSetter_) {
    lucent::error("ctr-runtime", "frame driver has no generated override setter");
    std::abort();
  }
  overrideSetter_(address, overrideFunction);
}

void CtrRuntime::dispatch(Core &core, uint32_t address) const {
  dispatch_(&core, address);
}

void CtrRuntime::runRecompiledSuper(Core &core, uint32_t address) const {
  if (!superDispatch_) {
    lucent::error("ctr-runtime", "frame driver has no generated super dispatcher");
    std::abort();
  }
  superDispatch_(&core, address);
}

} // namespace ctr
