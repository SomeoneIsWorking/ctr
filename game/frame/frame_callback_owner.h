#pragma once

#include <cstdint>

class Core;

namespace ctr {

// Supplies the display-field-paced hardware events the direct runtime does not generate.
class FrameCallbackOwner final {
public:
  void deliverField(Core &core) const;

private:
  void dispatchPreservingContext(Core &core, uint32_t callback, const char *kind) const;
};

} // namespace ctr
