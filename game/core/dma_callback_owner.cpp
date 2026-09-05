#include "dma_callback_owner.h"

#include "core.h"
#include "ctr_runtime.h"
#include "dma_irq.h"
#include "game.h"
#include "native_ownership.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

class CpuContextRestore final {
public:
  explicit CpuContextRestore(Core &core) : core_(core), saved_(core) {}

  ~CpuContextRestore() {
    static_cast<R3000 &>(core_) = saved_;
  }

  CpuContextRestore(const CpuContextRestore &) = delete;
  CpuContextRestore &operator=(const CpuContextRestore &) = delete;

private:
  Core &core_;
  R3000 saved_;
};

class IrqDeliveryGuard final {
public:
  explicit IrqDeliveryGuard(Core &core) : hle_(core.game->hle) {
    if (hle_.in_irq != 0) {
      lucent::error("ctr-dma", "attempted nested SPU DMA callback delivery");
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

DmaCallbackOwner::DmaCallbackOwner(DmaCompletionBackend backend) : backend_(backend) {
  if (!backend_.owed || !backend_.take || !backend_.ack) {
    lucent::error("ctr-dma", "SPU DMA owner requires complete owed/take/ack operations");
    std::abort();
  }
}

bool DmaCallbackOwner::hasPendingSpu() const {
  return backend_.owed(native::kSpuDmaChannel);
}

bool DmaCallbackOwner::serviceSpu(Core &core, const CtrRuntime &runtime) const {
  if (!hasPendingSpu()) {
    return false;
  }
  if (!core.game) {
    lucent::error("ctr-dma", "SPU DMA callback delivery requires a bound Game");
    std::abort();
  }
  // Match Hle::irqPoll's delivery ordering. A callback may start another synchronous DMA transfer;
  // its function-entry polls must see in_irq and leave that new completion owed for a later host
  // field, just as the BIOS would defer a completion which arrived inside the current IRQ.
  if (core.game->hle.in_irq != 0) {
    return false;
  }

  backend_.take(native::kSpuDmaChannel);
  backend_.ack(native::kSpuDmaChannel);
  const uint32_t callback = core.mem_r32(native::kSpuDmaCallbackSlot);
  if (callback == 0u) {
    return true;
  }

  const GuestProgramImage *image = runtime.guestProgramImage();
  if (!image || !image->residentText.containsPhysical(callback)) {
    lucent::error("ctr-dma", "SPU DMA callback 0x{:08X} is outside CTR's resident executable", callback);
    std::abort();
  }

  CpuContextRestore restore(core);
  IrqDeliveryGuard delivering(core);
  runtime.dispatchToReturn(core, callback, "CTR SPU DMA completion callback");
  return true;
}

DmaCompletionBackend DmaCallbackOwner::nativeBackend() {
  return {
      .owed = dma_done_owed,
      .take = dma_done_taken,
      .ack = dma_irq_ack,
  };
}

} // namespace ctr
