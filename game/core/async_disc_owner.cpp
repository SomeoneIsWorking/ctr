#include "async_disc_owner.h"

#include "cd_control.h"
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

void DiscReadOwner::noteTransferComplete(Core &core) {
  if (pending_) {
    // One guest drive cannot have two transfers in flight; the read leaf delivers the previous
    // completion before starting another, so reaching here means that ordering broke.
    lucent::error("ctr-disc", "a second native CdRead completed with the previous completion still owed");
    std::abort();
  }
  if (core.mem_r32(native::kCdReadCompletionCallback) == 0u) {
    // A caller which registered nothing waits through CdReadSync, which the shared native owner
    // already reports complete. Report the first one so "no callback delivered" is never
    // indistinguishable from a delivery that silently failed.
    if (++polledReads_ == 1u || lucent::channel_on("ctr-disc")) {
      lucent::info("ctr-disc",
                   "native CdRead {} completed with no registered libcd callback; its caller polls CdReadSync",
                   polledReads_);
    }
    return;
  }
  pending_ = true;
}

void DiscReadOwner::deliverPending(Core &core, const CtrRuntime &runtime) {
  if (!pending_) {
    return;
  }
  pending_ = false;
  const uint32_t callback = core.mem_r32(native::kCdReadCompletionCallback);
  if (callback == 0u) {
    // The guest cancelled its own callback before the interrupt could arrive; retail would deliver
    // nothing either. Counted as a polled read so the totals still account for every transfer.
    ++polledReads_;
    return;
  }

  // The callback runs as an ordinary guest function in place of the CD interrupt that would have
  // entered it, so the interrupted caller must see its whole register context unchanged.
  const R3000 interrupted = static_cast<const R3000 &>(core);
  core.r[4] = native::kCdlComplete;
  core.r[5] = 0u; // libcd passes its result bytes; neither measured CTR callback reads them
  runtime.dispatch(core, callback);
  static_cast<R3000 &>(core) = interrupted;
  if (++deliveredCallbacks_ == 1u || lucent::channel_on("ctr-disc")) {
    lucent::info("ctr-disc", "delivered libcd read-completion callback {} -> 0x{:08X}", deliveredCallbacks_, callback);
  }
}

DiscReadOwner &discReadOwner() {
  static DiscReadOwner owner;
  return owner;
}

void cdReadWithCompletionCallback(Core *core) {
  const GameRuntime *runtime = core->game ? core->game->runtime : nullptr;
  if (runtime == nullptr) {
    lucent::error("ctr-disc", "native CdRead has no bound CTR runtime to dispatch the completion callback");
    std::abort();
  }
  const CtrRuntime &ctrRuntime = *static_cast<const CtrRuntime *>(runtime);
  discReadOwner().deliverPending(*core, ctrRuntime);
  cd_read_stock_sync(core);
  if (core->r[2] == 0u) {
    // cd_read_stock_sync has already named the unreadable sector. Retail would deliver an error
    // callback and retry the drive; this port has no drive to retry, so stop rather than leave the
    // guest waiting on a completion that can never be honest.
    lucent::error("ctr-disc", "native CdRead failed; the retail completion callback cannot be delivered");
    std::abort();
  }
  discReadOwner().noteTransferComplete(*core);
}

} // namespace ctr
