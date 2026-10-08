#include "vsync_bridge.h"

#include "core.h"
#include "ctr_runtime.h"
#include "execution_services.h"
#include "retail_return.h"

namespace ctr {

void runVsyncBridge(Core &core, CtrRuntime &runtime, const VsyncBridge &bridge) {
  if (core.r[31] != bridge.expectedReturn) {
    refuseUnexpectedRetailReturn("VSync predecessor", bridge.expectedReturn, core.r[31]);
  }
  runtime.callOriginalToReturn(core, bridge.calleeAddress, "CTR VSync predecessor");
  // The call is omitted; its jal/delay-slot register effects are kept.
  core.r[31] = bridge.continuation;
  core.r[4] = bridge.modeArgument;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  runtime.propagateFrameBoundary(core, runtime.dispatch(core, bridge.continuation), "CTR post-VSync continuation");
}

} // namespace ctr
