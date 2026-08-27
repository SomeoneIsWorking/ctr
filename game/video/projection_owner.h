#pragma once

#include <cstdint>
#include <functional>

class Core;

namespace ctr {

struct ProjectionPublication {
  uint32_t source = 0;
  int32_t nativeWidth = 0;
  int32_t nativeHeight = 0;
  int32_t centerX = 0;
  int32_t centerY = 0;
  uint32_t screenDistance = 0;
  uint64_t sequence = 0;

  [[nodiscard]] bool valid() const {
    return source != 0 && nativeWidth > 0 && nativeHeight > 0 && screenDistance > 0;
  }
};

// Owns CTR's measured pre-GTE projection publication. The retail function remains the behavior
// oracle; this owner captures its view input and refuses if the published libgte state disagrees.
// Widescreen begins here later, once the native camera/culling owner can consume a wider plan.
class ProjectionOwner final {
public:
  using RetailBody = std::function<void(Core &)>;

  void publish(Core &core, const RetailBody &retailBody);

  [[nodiscard]] const ProjectionPublication &previous() const;
  [[nodiscard]] const ProjectionPublication &current() const;

private:
  ProjectionPublication previous_{};
  ProjectionPublication current_{};
};

} // namespace ctr
