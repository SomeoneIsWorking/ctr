#include "execution/spu_dma_callback_registration.h"

#include "core.h"
#include "execution/guest_turns.h"
#include "game.h"
#include "title/native_ownership.h"

namespace ctr {

void SpuDmaCallbackRegistration::service(Core &core) const {
  guest::callOriginalToReturn(core, native::kDmaCallback, "CTR DMACallback");
  core.game->dmaCallbacks.exchange(DmaChannel::Spu, core.mem_r32(native::kSpuDmaCallbackSlot));
}

} // namespace ctr
