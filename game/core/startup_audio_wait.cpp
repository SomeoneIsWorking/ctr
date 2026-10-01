#include "startup_audio_wait.h"

#include "core.h"
#include "ctr_runtime.h"
#include "dma_callback_owner.h"
#include "execution_exit.h"
#include "field_boundary.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

StartupAudioWait::StartupAudioWait(CtrRuntime &runtime, DmaCallbackOwner &dmaCallbacks, FieldBoundary &field)
    : runtime_(runtime), dmaCallbacks_(dmaCallbacks), field_(field) {}

bool StartupAudioWait::ownsContinuation() const {
  return continuationAddress_ != 0;
}

void StartupAudioWait::resumeLoop(Core &core) {
  dmaCallbacks_.serviceSpu(core, runtime_);
  if (dmaCallbacks_.hasPendingSpu(core)) {
    continuationAddress_ = native::kStartupAudioLoop;
    field_.request(core);
    return;
  }
  runtime_.propagateFrameBoundary(
      core,
      psx::cpu::callOriginalUntilExit(core, native::kStartupAudioLoop, psx::cpu::ExecutionBudget::currentTurn(core)),
      "CTR startup-audio loop");
}

void StartupAudioWait::service(Core &core) {
  const uint32_t caller = core.r[31];
  runtime_.callOriginalToReturn(core, native::kStartupAudioService, "CTR startup-audio service");
  if (caller != native::kStartupAudioServiceReturn) {
    return;
  }
  if (core.mem_r32(native::kStartupAudioWaitState) == 0u) {
    return;
  }
  if (ownsContinuation()) {
    lucent::error("ctr-frame", "nested state-zero audio waits are not supported");
    std::abort();
  }
  continuationAddress_ = native::kStartupAudioServiceReturn;
  field_.request(core);
}

void StartupAudioWait::resume(Core &core) {
  const uint32_t continuation = continuationAddress_;
  continuationAddress_ = 0;
  runtime_.propagateFrameBoundary(core, runtime_.dispatch(core, continuation), "CTR startup-audio continuation");
}

} // namespace ctr
