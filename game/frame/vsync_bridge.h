#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

// One of CTR's extracted VSync callsites, as data.
//
// Retail calls guest VSync from inside a function the title still executes. The title owns the
// TIMING, so it may not call it, but the callee's register effects, the branch it takes, and the
// work around it are the game's. This descriptor names all four facts the bridge needs, so a
// callsite is a table row a reader can check rather than five positional arguments whose order has
// to be remembered.
struct VsyncBridge {
  // The retail function whose body contains the VSync call. Its whole body is preserved.
  uint32_t calleeAddress = 0;
  // The ONE measured return address that identifies this callsite. Reaching the bridge from any
  // other return address is refused: it would mean the guest is on a path the transcription was not
  // written for.
  uint32_t expectedReturn = 0;
  // The first instruction after the omitted call. Execution resumes here, in the guest.
  uint32_t continuation = 0;
  // The value retail passes in a0 across the call. Startup and shutdown VSync calls are `VSync(0)`
  // and `VSync(30)` respectively; it is preserved because the callee reads it after the call.
  uint32_t modeArgument = 0;
};

// The jal plus its delay slot: the two guest instructions whose only effect is the call this owner
// omits, accounted so a field's instruction count still matches retail's.
inline constexpr uint32_t kOmittedCallInstructionCount = 2u;

// Preserves the retail callee, omits only its guest VSync call, restores the exact post-call
// register state, and resumes at the continuation. The result is required to be a frame boundary;
// anything else is a refusal, because the bridge exists to end a field.
void runVsyncBridge(Core &core, CtrRuntime &runtime, const VsyncBridge &bridge);

} // namespace ctr
