#include "spu_dma_callback_registration.h"

#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "native_ownership.h"

namespace ctr {

SpuDmaCallbackRegistration::SpuDmaCallbackRegistration(CtrRuntime &runtime) : runtime_(runtime) {}

void SpuDmaCallbackRegistration::service(Core &core) const {
  runtime_.callOriginalToReturn(core, native::kDmaCallback, "CTR DMACallback");
  core.game->dmaCallbacks.exchange(DmaChannel::Spu, core.mem_r32(native::kSpuDmaCallbackSlot));
}

} // namespace ctr
