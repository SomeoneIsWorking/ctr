#include "disc/async_disc_owner.h"

#include "cd_drive_timing.h"
#include "core.h"
#include "execution/guest_turns.h"
#include "game.h"
#include "runtime/runtime.h"
#include "runtime/session.h"
#include "title/native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

void DiscReadOwner::noteTransferComplete(Core &core,
                                         std::optional<CompletedDiscRead> read,
                                         uint32_t sectors,
                                         uint8_t mode) {
  if (pending_) {
    // One drive cannot have two transfers in flight; the read leaf delivers the previous one first.
    lucent::error("ctr-disc", "a second native CdRead completed with the previous completion still owed");
    std::abort();
  }
  if (read && !overlayImages_.observeCompletedRead(
                  core, *read, core.mem_r32(native::kCdReadCompletionCallback) == native::kBigfileCompletionCallback)) {
    lucent::error("ctr-disc", "completed CdRead did not satisfy the title image contract");
    std::abort();
  }
  if (core.mem_r32(native::kCdReadCompletionCallback) == 0u) {
    // A caller that registered nothing waits through CdReadSync, already reported complete.
    ++polledReads_;
    lucent::info("ctr-disc",
                 "native CdRead {} completed with no registered libcd callback; its caller polls CdReadSync",
                 polledReads_);
    return;
  }
  pending_ = true;
  dueTicks_ = core.game->timing.emulatedCpuTicks() + uint64_t{sectors} * psx::cd::driveSectorPeriodCpuTicks(mode);
}

void DiscReadOwner::deliverIfDue(Core &core) {
  if (pending_ && core.game->timing.emulatedCpuTicks() >= dueTicks_) {
    deliverPending(core);
  }
}

void DiscReadOwner::deliverPending(Core &core) {
  if (!pending_) {
    return;
  }
  pending_ = false;
  const auto imageCandidate = overlayImages_.takePending();
  const uint32_t callback = core.mem_r32(native::kCdReadCompletionCallback);
  if (callback == 0u) {
    // The guest cancelled its callback; retail would deliver nothing. Counted as a polled read.
    ++polledReads_;
    return;
  }

  // Stands in for the CD interrupt, so the interrupted caller's registers must be unchanged.
  const R3000 interrupted = static_cast<const R3000 &>(core);
  core.r[4] = native::kCdlComplete;
  core.r[5] = 0u; // libcd result bytes; no CTR callback reads them
  guest::callToReturn(core, callback, "CTR libcd completion callback");
  overlayImages_.publishAfterCallback(core, imageCandidate);
  static_cast<R3000 &>(core) = interrupted;
  ++deliveredCallbacks_;
  lucent::info("ctr-disc", "delivered libcd read-completion callback {} -> 0x{:08X}", deliveredCallbacks_, callback);
}

void readWithCompletionCallback(Core *core) {
  auto &completion = session(*core).discRead;
  completion.deliverPending(*core);
  const CompletedDiscRead read{static_cast<uint32_t>(core->game->cd.setloc_lba), core->r[4], core->r[5], core->r[6]};
  Runtime::from(*core).libcd().read(*core);
  if (core->r[2] == 0u) {
    // No drive to retry on an unreadable sector; stop rather than wait on a completion that cannot come.
    lucent::error("ctr-disc", "native CdRead failed; the retail completion callback cannot be delivered");
    std::abort();
  }
  completion.noteTransferComplete(*core, read, read.sectors, static_cast<uint8_t>(read.mode));
}

} // namespace ctr
