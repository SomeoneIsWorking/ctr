#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

struct VsyncBridge {
  uint32_t calleeAddress = 0;
  // Any other return address is refused.
  uint32_t expectedReturn = 0;
  uint32_t continuation = 0;
  // a0 retail passes across the call; the callee reads it after.
  uint32_t modeArgument = 0;
};

// The jal and its delay slot, counted so the field's instruction count matches retail.
inline constexpr uint32_t kOmittedCallInstructionCount = 2u;

// Runs the retail callee without its guest VSync call and resumes at the continuation; must end in a frame boundary.
void runVsyncBridge(Core &core, CtrRuntime &runtime, const VsyncBridge &bridge);

} // namespace ctr
