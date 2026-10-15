#include "frame/vsync_bridge.h"

#include "core.h"
#include "execution/guest_turns.h"
#include "execution_services.h"
#include "frame/retail_return.h"

namespace ctr {

void runVsyncBridge(Core &core, const VsyncBridge &bridge) {
  if (core.r[31] != bridge.expectedReturn) {
    refuseUnexpectedRetailReturn("VSync predecessor", bridge.expectedReturn, core.r[31]);
  }
  guest::callOriginalToReturn(core, bridge.calleeAddress, "CTR VSync predecessor");
  // The call is omitted; its jal/delay-slot register effects are kept.
  core.r[31] = bridge.continuation;
  core.r[4] = bridge.modeArgument;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  guest::propagateFrameBoundary(core, guest::dispatch(core, bridge.continuation), "CTR post-VSync continuation");
}

} // namespace ctr
