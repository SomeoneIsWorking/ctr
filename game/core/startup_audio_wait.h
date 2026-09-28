#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class DmaCallbackOwner;
class FieldBoundary;

// State zero's post-archive XA wait.
//
// After the startup sound archive loads, state zero waits on the XA task at 0x8008D708. In retail
// that wait is a loop the host cannot observe, and the decoded ring behind it fills whether or not
// the field advances: a host that does not move audio forward cannot leave the startup clip. This
// owner therefore serves the retail service call exactly as written, then yields ONE host field —
// the audio field — per call, and resumes at the retail poll that follows.
//
// The wait's own state is the single continuation address, because retail reaches the wait from one
// exact site and there is nothing else to remember between fields.
class StartupAudioWait final {
public:
  StartupAudioWait(CtrRuntime &runtime, DmaCallbackOwner &dmaCallbacks, FieldBoundary &field);

  // True while this owner holds a continuation to resume at the start of a field.
  [[nodiscard]] bool ownsContinuation() const;

  // The wrapper for the retail loop. Delivers one owed SPU callback first, because a channel-4
  // completion created inside Hle::irqPoll's custom-exception unwind is deliverable now while the
  // direct runtime's generic DMA table is intentionally absent. If that callback starts the next
  // synchronous transfer, the field is yielded first: hardware would not complete both transfers in
  // one interrupt, and translated code must not consume a completion with no callback behind it.
  void resumeLoop(Core &core);

  // The retail service call the loop makes. Preserved verbatim, then one field is yielded while the
  // task is still running.
  void service(Core &core);

  // Resumes the continuation the yielded field owed.
  void resume(Core &core);

private:
  CtrRuntime &runtime_;
  DmaCallbackOwner &dmaCallbacks_;
  FieldBoundary &field_;
  uint32_t continuationAddress_ = 0;
};

} // namespace ctr
