#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// The three guest words the retail frame suffix at 0x80037880 waits on.
struct FrameSuffixWait {
  // Zero means none is published, which is not a pending draw.
  uint32_t gameState = 0;
  // Nonzero while the published draw is outstanding.
  bool drawSyncPending = false;
  bool vblankFieldsPending = false;
  uint32_t registeredCallbacks = 0;
};

[[nodiscard]] bool frameSuffixStillWaiting(const FrameSuffixWait &wait);

// The per-field frame-timing bridge and the retail suffix that closes the field.
class FrameSuffix final {
public:
  FrameSuffix(CtrRuntime &runtime, FieldBoundary &field);

  [[nodiscard]] bool isPending() const;

  // The timing leaf's conditional debug VSync(0) is omitted.
  void completeFieldTiming(Core &core);

  void resume(Core &core);

private:
  [[nodiscard]] FrameSuffixWait readWaitState(Core &core) const;

  CtrRuntime &runtime_;
  FieldBoundary &field_;
  bool pending_ = false;
};

} // namespace ctr
