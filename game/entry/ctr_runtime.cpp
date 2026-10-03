#include "ctr_runtime.h"

#include "core.h"
#include "execution_control.h"
#include "frame_driver.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "platform_hle_plan.h"
#include "resumable_guest_call.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {

CtrRuntime::CtrRuntime(uint32_t bootTarget) : bootTarget_(bootTarget) {
  if (bootTarget_ == 0) {
    lucent::error("ctr-runtime", "runtime requires a nonzero independently validated boot target");
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
  // Frame-scoped title overrides are installed by CtrFrameDriver after the executable image is active.
}

void CtrRuntime::bootInit(Core &) {
  // CTR boot contains host-owned waits that can span multiple fields. CtrFrameDriver is the single
  // cooperative boot owner; this framework compatibility hook must not enter the executable a
  // second time before the first finite field step.
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

const GuestWidescreenProjection *CtrRuntime::guestWidescreenProjection() const {
  return &widescreen_;
}

CtrWidescreen &CtrRuntime::widescreen() {
  return widescreen_;
}

bool CtrRuntime::guestVramIsPicture(const Game &) const {
  // The title driver preserves the retail frame order but does not present guest VRAM or own a
  // native primitive renderer. Claiming picture content would invent a presentation path.
  return false;
}

psx::cpu::ExecutionResult CtrRuntime::dispatch(Core &core, uint32_t address) const {
  return psx::cpu::dispatchGuestUntilExit(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
}

uint32_t CtrRuntime::callToContinuation(Core &core, uint32_t address, uint32_t returnPc, std::string_view owner) const {
  // A state-zero body may legitimately outlive a display field, so the cap is stated by the CALLER
  // (kUnboundedCallTurns) rather than guessed here.
  return psx::cpu::callGuestToReturnResuming(
      core, owner, address, returnPc, std::nullopt, psx::cpu::kUnboundedCallTurns);
}

psx::cpu::ExecutionResult CtrRuntime::dispatchHop(Core &core, uint32_t address, uint32_t returnPc) const {
  return core.lightrecExecutor().executeFunction(address, returnPc, psx::cpu::ExecutionBudget::currentTurn(core));
}

void CtrRuntime::callToReturn(Core &core, uint32_t address, std::string_view owner) const {
  psx::cpu::callGuestToReturnResuming(core, owner, address, core.r[31], std::nullopt, psx::cpu::kDefaultCallTurns);
}

void CtrRuntime::callOriginalToReturn(Core &core, uint32_t address, std::string_view owner) const {
  psx::cpu::callOriginalResumingToReturn(core, owner, address, core.r[31], psx::cpu::kDefaultCallTurns);
}

void CtrRuntime::propagateFrameBoundary(Core &core,
                                        const psx::cpu::ExecutionResult &result,
                                        std::string_view owner) const {
  if (result.reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    lucent::error("ctr-runtime",
                  "{} left guest execution at 0x{:08X} with {} instead of a frame boundary",
                  owner,
                  result.guestPc,
                  psx::cpu::executionExitName(result.reason));
    std::abort();
  }
  psx::cpu::requestExecutionExit(core, result);
}

} // namespace ctr
