#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

// The framework's DMA completion hooks (dma_irq.h) take the Core, because owed/taken state and the
// DICR acknowledgement both live per-instance on it. This table mirrors that contract exactly.
struct DmaCompletionBackend {
  bool (*owed)(const Core &core, int channel);
  void (*take)(Core &core, int channel);
  void (*ack)(Core &core, int channel);
};

// Delivers CTR's measured SPU DMA callback from its guest callback table. Direct runtimes do not
// expose the legacy GameConfig DMA-table field, so this title owner services only channel 4 at the
// finite host boundary before the generic IRQ path can consume an undeliverable completion.
class DmaCallbackOwner final {
public:
  explicit DmaCallbackOwner(DmaCompletionBackend backend = nativeBackend());

  [[nodiscard]] bool hasPendingSpu(const Core &core) const;
  bool serviceSpu(Core &core, const CtrRuntime &runtime) const;

  static DmaCompletionBackend nativeBackend();

private:
  DmaCompletionBackend backend_;
};

} // namespace ctr
