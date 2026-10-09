#include "frame_callback_owner.h"

#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

namespace {

class IrqDeliveryGuard final {
public:
  explicit IrqDeliveryGuard(Core &core) : hle_(core.game->hle) {
    if (hle_.in_irq != 0) {
      lucent::error("ctr-field", "attempted nested native field callback delivery");
      std::abort();
    }
    hle_.in_irq = 1;
  }

  ~IrqDeliveryGuard() {
    hle_.in_irq = 0;
  }

  IrqDeliveryGuard(const IrqDeliveryGuard &) = delete;
  IrqDeliveryGuard &operator=(const IrqDeliveryGuard &) = delete;

private:
  Hle &hle_;
};

} // namespace

void FrameCallbackOwner::deliverField(Core &core, const CtrRuntime &runtime) const {
  if (!core.game) {
    lucent::error("ctr-field", "native field callback delivery requires a bound Game");
    std::abort();
  }
  if (core.game->hle.in_irq != 0) {
    return;
  }

  // 0x80034A80 (DrawSyncCallback) clears this flag; native draws complete synchronously,
  // so a flag still armed at the next field is an owed callback.
  const uint32_t gameState = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  if (gameState != 0u && core.mem_r8(gameState + native::kDrawSyncPendingOffset) == 1u) {
    dispatchPreservingContext(core, runtime, core.mem_r32(native::kDrawSyncCallbackSlot), "DrawSyncCallback");
  }
  // A suffix wait runs no guest code, so no function entry polls the vblank edge for libetc's ISR.
  if ((core.pending_work & Core::PW_IRQ) != 0u) {
    core.game->hle.irqPoll(&core);
  }
}

void FrameCallbackOwner::dispatchPreservingContext(Core &core,
                                                   const CtrRuntime &runtime,
                                                   uint32_t callback,
                                                   const char *kind) const {
  if (callback == 0u) {
    return;
  }
  const GuestProgramImage *image = runtime.guestProgramImage();
  if (!image || !image->residentText.containsPhysical(callback)) {
    lucent::error("ctr-field", "{} 0x{:08X} is outside CTR's resident executable", kind, callback);
    std::abort();
  }

  const R3000 saved = static_cast<R3000 &>(core);
  {
    IrqDeliveryGuard delivering(core);
    runtime.callToReturn(core, callback, kind);
  }
  static_cast<R3000 &>(core) = saved;
}

} // namespace ctr
