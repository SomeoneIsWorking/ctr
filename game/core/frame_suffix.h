#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// The three guest words the retail frame suffix waits on, read once and named.
//
// The suffix at 0x80037880 is retail code and stays retail code. What the host owns is WHEN to let
// it run: it must not run while the draw the field just published is still being flushed, nor while
// the VSync callback's two-field countdown is running. Those are three guest words, and a reader
// should be able to see the predicate's inputs without redoing the `gp`-relative arithmetic.
struct FrameSuffixWait {
  // The game-state descriptor the guest's own code passes around. Zero means the guest has not
  // published one, and a zero descriptor is not a pending draw.
  uint32_t gameState = 0;
  // The DrawSync callback's byte: nonzero while the just-published draw is still outstanding.
  bool drawSyncPending = false;
  // The VSync callback's field countdown, read as a signed count of fields still to wait.
  bool vblankFieldsPending = false;
  // How many callbacks the guest has registered so far.
  uint32_t registeredCallbacks = 0;
};

// The predicate itself, as a pure function of the three words. It is a namespace function because it
// has no state: the guest reads live in the owner below, and the rule lives here where a test can
// reach it without a Core.
[[nodiscard]] bool frameSuffixStillWaiting(const FrameSuffixWait &wait);

// The per-field frame-timing bridge and the retail frame suffix that closes the field.
//
// State 3 calls one frame/presentation owner per iteration. Its translated retail prefix reaches the
// unconditional timing calculation, which the title runs verbatim; the only thing removed is the
// CONDITIONAL debug VSync(0) query, because the host owns timing and the query is a read of it. The
// suffix that follows is entered directly and remains the authority once its wait predicate becomes
// false — this owner decides only when to let it run, and when the field is over.
class FrameSuffix final {
public:
  FrameSuffix(CtrRuntime &runtime, FieldBoundary &field);

  // True while a suffix is owed but has not yet run to its end.
  [[nodiscard]] bool isPending() const;

  // Runs the timing leaf retail wrote, omits only the conditional debug VSync(0), and then runs the
  // suffix — or yields the field again if the suffix still has work to wait for.
  void completeFieldTiming(Core &core);

  // Runs an owed suffix at the start of a field.
  void resume(Core &core);

private:
  [[nodiscard]] FrameSuffixWait readWaitState(Core &core) const;

  CtrRuntime &runtime_;
  FieldBoundary &field_;
  bool pending_ = false;
};

} // namespace ctr
