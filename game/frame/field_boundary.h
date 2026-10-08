#pragma once

#include "presentation_owner.h"

#include <cstdint>

class Core;

namespace ctr {

// Ends a CTR field by the title's own request or the executor's typed frame-boundary exit.
class FieldBoundary final {
public:
  explicit FieldBoundary(PresentationOwner &presentation);

  // Clears the title's request before any owner can raise a new one.
  void beginField();

  void request(Core &core);

  // Refuses a pending typed exit that is not a frame boundary.
  [[nodiscard]] bool pending(Core &core) const;

  void finishField(Core &core, uint32_t field);

  [[nodiscard]] uint32_t completedFields() const;

private:
  PresentationOwner &presentation_;
  uint32_t completedFields_ = 0;
  bool requested_ = false;
};

} // namespace ctr
