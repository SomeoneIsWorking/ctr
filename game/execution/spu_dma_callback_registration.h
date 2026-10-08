#pragma once

class Core;

namespace ctr {

class CtrRuntime;

// Runs the retail DMACallback and publishes the SPU channel's guest callback to the framework registry,
// so the shared interrupt path delivers its completions to the callback the guest registered.
class SpuDmaCallbackRegistration final {
public:
  explicit SpuDmaCallbackRegistration(CtrRuntime &runtime);

  void service(Core &core) const;

private:
  CtrRuntime &runtime_;
};

} // namespace ctr
