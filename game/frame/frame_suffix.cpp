#include "frame_suffix.h"

#include "core.h"
#include "ctr_runtime.h"
#include "execution_exit.h"
#include "execution_services.h"
#include "field_boundary.h"
#include "native_ownership.h"
#include "retail_return.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// Word the timing leaf's result is stored into, relative to s4; its meaning is unidentified.
inline constexpr uint32_t kUnattributedWordOffsetFromS4 = 7388u;

// Debug flag bit 12 in the game-state descriptor; the leaf calls VSync(0) when the word is nonzero.
inline constexpr uint32_t kDebugVSyncFlagWordOffset = 9580u;
inline constexpr uint32_t kDebugVSyncFlagMask = 4096u;

// The omitted call is VSync(0), a query.
inline constexpr uint32_t kDebugVSyncMode = 0u;

// Guest instruction counts of the two bridge shapes.
inline constexpr uint32_t kBridgeInstructionCountWithDebugVSync = 9u;
inline constexpr uint32_t kBridgeInstructionCountWithoutDebugVSync = 7u;

// The suffix yields until this many callbacks are registered; likely the seven-entry libapi DMA
// callback table in native_ownership.h, unconfirmed.
inline constexpr uint32_t kRegisteredCallbackWaitLimit = 7u;

} // namespace

bool frameSuffixStillWaiting(const FrameSuffixWait &wait) {
  if (wait.gameState == 0u) {
    return false;
  }
  return (wait.drawSyncPending || wait.vblankFieldsPending) && wait.registeredCallbacks < kRegisteredCallbackWaitLimit;
}

FrameSuffix::FrameSuffix(CtrRuntime &runtime, FieldBoundary &field) : runtime_(runtime), field_(field) {}

bool FrameSuffix::isPending() const {
  return pending_;
}

void FrameSuffix::completeFieldTiming(Core &core) {
  if (core.r[31] != native::kFrameTimingReturn) {
    refuseUnexpectedRetailReturn("frame timing super", native::kFrameTimingReturn, core.r[31]);
  }

  runtime_.callOriginalToReturn(core, native::kFrameTiming, "CTR frame timing");

  // 0x8003785C..0x80037880: the conditional VSync(0) is omitted, its register effects kept.
  core.r[3] = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  core.mem_w32(core.r[20] + kUnattributedWordOffsetFromS4, core.r[2]);
  core.r[2] = core.mem_r32(core.r[3] + kDebugVSyncFlagWordOffset);
  core.r[2] &= kDebugVSyncFlagMask;
  const bool debugVSync = core.r[2] != 0;
  psx::cpu::accountGuestInstructions(
      core, debugVSync ? kBridgeInstructionCountWithDebugVSync : kBridgeInstructionCountWithoutDebugVSync);
  if (debugVSync) {
    // Register effects of the jal/delay slot at 0x80037878/0x8003787C; the call itself is omitted.
    core.r[31] = native::kFrameSuffix;
    core.r[4] = kDebugVSyncMode;
  }

  pending_ = true;
  resume(core);
}

void FrameSuffix::resume(Core &core) {
  if (!pending_) {
    lucent::error("ctr-frame", "frame suffix resumed without an owned continuation");
    std::abort();
  }
  if (frameSuffixStillWaiting(readWaitState(core))) {
    field_.request(core);
    return;
  }
  runtime_.callToContinuation(core, native::kFrameSuffix, native::kFrameLoopResume, "CTR frame suffix");
  if (core.r[31] != native::kFrameLoopResume) {
    refuseUnexpectedRetailReturn("frame suffix", native::kFrameLoopResume, core.r[31]);
  }
  pending_ = false;
  field_.request(core);
}

FrameSuffixWait FrameSuffix::readWaitState(Core &core) const {
  FrameSuffixWait wait;
  wait.gameState = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  if (wait.gameState == 0u) {
    return wait;
  }
  wait.drawSyncPending = core.mem_r8(wait.gameState + native::kDrawSyncPendingOffset) != 0u;
  wait.vblankFieldsPending = static_cast<int32_t>(core.mem_r32(core.r[28] + native::kFrameWaitFieldsGpOffset)) > 0;
  wait.registeredCallbacks = core.mem_r32(wait.gameState + native::kFrameCallbackCountOffset);
  return wait;
}

} // namespace ctr
