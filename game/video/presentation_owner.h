#pragma once

#include <cstdint>

class Core;

namespace ctr {

class SceneCut;

// Owns the framework presentation fence at each host field; the record path seals and presents every field.
class PresentationOwner final {
public:
  explicit PresentationOwner(SceneCut &sceneCut);

  void finishField(Core &core);

  [[nodiscard]] uint64_t completedFences() const;

private:
  SceneCut &sceneCut_;
  uint64_t completedFences_ = 0;
};

} // namespace ctr
