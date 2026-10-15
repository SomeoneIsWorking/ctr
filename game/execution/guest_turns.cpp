#include "execution/guest_turns.h"

#include "core.h"
#include "execution_control.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr::guest {

psx::cpu::ExecutionResult dispatch(Core &core, uint32_t address) {
  return psx::cpu::dispatchGuestUntilExit(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
}

uint32_t callToContinuation(Core &core, uint32_t address, uint32_t returnPc, std::string_view owner) {
  return psx::cpu::callGuestToReturnResuming(
      core, owner, address, returnPc, std::nullopt, psx::cpu::kUnboundedCallTurns);
}

psx::cpu::ExecutionResult dispatchHop(Core &core, uint32_t address, uint32_t returnPc) {
  return core.lightrecExecutor().executeFunction(address, returnPc, psx::cpu::ExecutionBudget::currentTurn(core));
}

void callToReturn(Core &core, uint32_t address, std::string_view owner) {
  psx::cpu::callGuestToReturnResuming(core, owner, address, core.r[31], std::nullopt, psx::cpu::kDefaultCallTurns);
}

void callOriginalToReturn(Core &core, uint32_t address, std::string_view owner) {
  psx::cpu::callOriginalResumingToReturn(core, owner, address, core.r[31], psx::cpu::kDefaultCallTurns);
}

void propagateFrameBoundary(Core &core, const psx::cpu::ExecutionResult &result, std::string_view owner) {
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

} // namespace ctr::guest
