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

// The word the timing leaf's result is stored into, relative to the base the frame holds in s4. This
// store is reproduced because the guest reads it back, but nothing in this file says what the word
// IS: the image has not been read for it, so the name records the offset and nothing more. It is
// worth noting only as a lead that it sits four bytes below `kFrameCallbackCountOffset` of the
// game-state descriptor — a relationship, not an identification.
inline constexpr uint32_t kUnattributedWordOffsetFromS4 = 7388u;

// The debug flag the timing leaf reads to decide whether to call VSync(0) at all. The mask is a
// single bit (bit 12) and the leaf branches on the word being nonzero, so one bit decides the whole
// call. Offset is relative to the game-state descriptor, which the leaf loads into v1 first.
inline constexpr uint32_t kDebugVSyncFlagWordOffset = 9580u;
inline constexpr uint32_t kDebugVSyncFlagMask = 4096u;

// The omitted call is VSync(0): a query, not a wait, so it carries no mode beyond the literal.
inline constexpr uint32_t kDebugVSyncMode = 0u;

// Guest instruction counts for the two shapes of the bridge. They are retail's: omitting the
// conditional query must not make a field look cheaper than retail ran it.
inline constexpr uint32_t kBridgeInstructionCountWithDebugVSync = 9u;
inline constexpr uint32_t kBridgeInstructionCountWithoutDebugVSync = 7u;

// The suffix keeps yielding until this many callbacks are registered. The threshold is UNTITLED in
// this file — the image has not been read for the comparison it comes from. It equals the size of
// the seven-entry libapi DMA callback table documented in native_ownership.h, which is a lead worth
// following and not a confirmation.
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

  // Exact 0x8003785C..0x80037880 bridge from SCUS_944.26. It retains the result store, debug-flag
  // read, and guest instruction accounting, but deliberately omits the conditional VSync(0) call.
  core.r[3] = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  core.mem_w32(core.r[20] + kUnattributedWordOffsetFromS4, core.r[2]);
  core.r[2] = core.mem_r32(core.r[3] + kDebugVSyncFlagWordOffset);
  core.r[2] &= kDebugVSyncFlagMask;
  const bool debugVSync = core.r[2] != 0;
  psx::cpu::accountGuestInstructions(
      core, debugVSync ? kBridgeInstructionCountWithDebugVSync : kBridgeInstructionCountWithoutDebugVSync);
  if (debugVSync) {
    // These are the jal/delay-slot register effects at 0x80037878/0x8003787C. The host still owns
    // timing, so the call itself is deliberately absent.
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
