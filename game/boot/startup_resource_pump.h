#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class DmaCallbackOwner;
class FieldBoundary;

// State zero's first resource continuation, whose retail polls never return to the host.
//
// `0x8002DD24` polls asynchronous CD and GPU stages in two `do/while` loops. Each poll can wait
// longer than a host field, so running it to completion would hand the field over to a loop the
// host cannot observe, and running it "a bit" is not a state the executor has. This owner keeps
// the retail stage functions executing through Lightrec, owns the loop's phase so it survives
// across fields, and yields one host field after each false poll.
//
// It also owns the ordering that makes those yields safe: owed SPU DMA work is delivered BEFORE the
// generic direct-runtime IRQ path, because the direct runtime has no legacy callback-table view and
// would consume a completion it cannot deliver.
class StartupResourcePump final {
public:
  // Where the two retail loops had got to. `Inactive` means this owner owns nothing.
  enum class Phase : uint8_t {
    Inactive,
    ResourcePoll,
    CommitPoll,
  };

  StartupResourcePump(CtrRuntime &runtime, DmaCallbackOwner &dmaCallbacks, FieldBoundary &field);

  [[nodiscard]] bool isActive() const;
  [[nodiscard]] Phase phase() const;

  // Enters the continuation and runs the first poll immediately, because the caller that reached
  // `0x8002DD24` did so from a field that had just been entered.
  void begin(Core &core);

  // Continues from the phase this owner holds. Yields the field on a false poll, and completes the
  // continuation when the commit poll reports progress.
  void resume(Core &core);

private:
  // Delivers owed work before a poll and reports whether that already ended the field.
  [[nodiscard]] bool serviceOwedWork(Core &core);
  void servicePendingWork(Core &core);
  void pollResources(Core &core);
  void pollCommit(Core &core);
  void finish(Core &core);

  CtrRuntime &runtime_;
  DmaCallbackOwner &dmaCallbacks_;
  FieldBoundary &field_;
  Phase phase_ = Phase::Inactive;
};

} // namespace ctr
