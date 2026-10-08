#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

// Supplies the display-field-paced hardware events the direct runtime does not generate.
class FrameCallbackOwner final {
public:
  void observeVblankRegistration(Core &core, const CtrRuntime &runtime);
  void deliverField(Core &core, const CtrRuntime &runtime) const;

private:
  void dispatchPreservingContext(Core &core, const CtrRuntime &runtime, uint32_t callback, const char *kind) const;

  uint32_t vblankCallback_ = 0;
};

} // namespace ctr
