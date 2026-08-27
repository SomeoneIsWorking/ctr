#include "async_disc_owner.h"

#include "core.h"
#include "ctr_runtime.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

void AsyncDiscOwner::startRead(Core &core, const CtrRuntime &runtime) const {
  runtime.runRecompiledSuper(core, native::kAsyncDiscRead);
  if (core.r[2] == 0u) {
    return;
  }

  const uint32_t completionStateAddress = core.r[28] + native::kAsyncDiscCompletionStateGpOffset;
  const uint32_t completionState = core.mem_r32(completionStateAddress);
  if (completionState != native::kAsyncDiscAwaitingCallback) {
    lucent::error("ctr-disc",
                  "synchronous read completed with unexpected retail callback state {} at 0x{:08X}",
                  completionState,
                  completionStateAddress);
    std::abort();
  }

  // SCUS_944.26 callback 0x8003254C unregisters itself and changes state 1 -> 0 when libcd reports
  // status 2. The native CdRead owner has already transferred the requested sectors before the
  // generated wrapper returns, so publish those two callback effects without inventing an IRQ.
  core.mem_w32(native::kCdReadyCallback, 0u);
  core.mem_w32(completionStateAddress, native::kAsyncDiscComplete);
}

} // namespace ctr
