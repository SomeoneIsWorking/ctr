#include "startup_audio_wait.h"

#include "core.h"
#include "ctr_runtime.h"
#include "execution_exit.h"
#include "field_boundary.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

StartupAudioWait::StartupAudioWait(CtrRuntime &runtime, FieldBoundary &field) : runtime_(runtime), field_(field) {}

bool StartupAudioWait::ownsContinuation() const {
  return continuationAddress_ != 0;
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
