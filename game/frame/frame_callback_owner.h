#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;

// Supplies the display-field-paced hardware events the direct runtime does not generate.
class FrameCallbackOwner final {
public:
  void deliverField(Core &core, const CtrRuntime &runtime) const;

private:
  void dispatchPreservingContext(Core &core, const CtrRuntime &runtime, uint32_t callback, const char *kind) const;
};

} // namespace ctr
