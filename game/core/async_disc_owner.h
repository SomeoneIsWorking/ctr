#pragma once

#include <cstdint>

struct Core;

namespace ctr {

class CtrRuntime;

// Stock Sony libcd ends a CdRead from the CD interrupt: the caller registers a completion function
// through CdReadCallback (0x800771B0, slot 0x8008AD10) and libcd invokes it with CdlComplete.
// psxport transfers every requested sector before the native CdRead leaf returns, so that interrupt
// never happens, and a caller which waits for the callback instead of polling CdReadSync waits
// forever.
//
// MEASURED on SCUS_944.26: the resource loader at 0x800321B4 registers 0x80032110 and the slot is
// never written again, so its busy flag at gp+0x138 stays 1, the screen loader at 0x80033610 keeps
// returning stage 2 unchanged, the ordering table stays empty, and every presented field is black.
//
// WHEN the callback runs is as much a part of the contract as whether it runs. Retail cannot deliver
// it inside CdRead, and the loader depends on that: FUN_80031E00 stores the allocated buffer into
// its queue entry AFTER FUN_800321B4 returns, and the completion chain reads that same field. So the
// owner records that a completion is OWED and delivers it at a seam where the issuing call has
// unwound — the title's per-field service point, or the next read, because one drive cannot have two
// transfers in flight. It transcribes no callback effects: the retail body owns them.
class DiscReadOwner final {
public:
  // Records that a transfer finished with a callback registered, or counts a read whose caller polls
  // CdReadSync instead. The caller must have delivered any owed completion first: one guest drive
  // cannot have two transfers in flight, and this refuses rather than losing the earlier one.
  void noteTransferComplete(Core &core);

  // Runs the registered libcd completion callback with the measured success code, restoring the
  // interrupted register context exactly as a real CD interrupt entry would. Does nothing when no
  // completion is owed.
  void deliverPending(Core &core, const CtrRuntime &runtime);

  [[nodiscard]] bool hasPendingCompletion() const {
    return pending_;
  }
  [[nodiscard]] uint32_t deliveredCallbacks() const {
    return deliveredCallbacks_;
  }
  [[nodiscard]] uint32_t polledReads() const {
    return polledReads_;
  }

private:
  bool pending_ = false;
  uint32_t deliveredCallbacks_ = 0;
  uint32_t polledReads_ = 0;
};

// The process owns one guest CD drive, so it owns one pending-completion state. The PlatformHle
// binding and the title frame driver reach the same owner through this.
DiscReadOwner &discReadOwner();

// PlatformHle binding for the measured stock libcd CdRead leaf: the shared synchronous transfer,
// then an owed completion callback.
void cdReadWithCompletionCallback(Core *core);

} // namespace ctr
