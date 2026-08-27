#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

// Owns CTR's callbacks which are paced by the native display field. The retail registration
// routines remain authoritative; this owner only supplies the hardware events which the direct
// runtime deliberately does not generate.
class FrameCallbackOwner final {
public:
  void observeVblankRegistration(Core &core, const CtrRuntime &runtime);
  void deliverField(Core &core, const CtrRuntime &runtime) const;

  [[nodiscard]] uint32_t vblankCallback() const;

private:
  void dispatchPreservingContext(Core &core, const CtrRuntime &runtime, uint32_t callback, const char *kind) const;

  uint32_t vblankCallback_ = 0;
};

} // namespace ctr
