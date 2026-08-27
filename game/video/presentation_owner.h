#pragma once

#include <cstdint>

class Core;

namespace ctr {

// Owns the framework presentation fence at each finite host field. A retail DrawOTag capture is
// real picture input and is committed through the GTE presenter; fields with no captured work stay
// explicitly unpresented rather than manufacturing an empty picture.
class PresentationOwner final {
public:
  void finishField(Core &core);

  [[nodiscard]] bool hasPresentableCapture(const Core &core) const;
  [[nodiscard]] uint64_t completedFences() const;
  [[nodiscard]] uint64_t presentedFences() const;

private:
  uint64_t completedFences_ = 0;
  uint64_t presentedFences_ = 0;
};

} // namespace ctr
