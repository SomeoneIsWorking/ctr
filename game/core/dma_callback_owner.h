#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

struct DmaCompletionBackend {
  bool (*owed)(int channel);
  void (*take)(int channel);
  void (*ack)(int channel);
};

// Delivers CTR's measured SPU DMA callback from its guest callback table. Direct runtimes do not
// expose the legacy GameConfig DMA-table field, so this title owner services only channel 4 at the
// finite host boundary before the generic IRQ path can consume an undeliverable completion.
class DmaCallbackOwner final {
public:
  explicit DmaCallbackOwner(DmaCompletionBackend backend = nativeBackend());

  [[nodiscard]] bool hasPendingSpu() const;
  bool serviceSpu(Core &core, const CtrRuntime &runtime) const;

  static DmaCompletionBackend nativeBackend();

private:
  DmaCompletionBackend backend_;
};

} // namespace ctr
