#pragma once

class Core;

namespace ctr {

// Runs the retail DMACallback and publishes the SPU channel's guest callback to the framework registry,
// so the shared interrupt path delivers its completions to the callback the guest registered.
class SpuDmaCallbackRegistration final {
public:
  void service(Core &core) const;
};

} // namespace ctr
