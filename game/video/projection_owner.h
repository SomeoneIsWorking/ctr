#pragma once

#include "view_projection_plan.h"

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
// oracle; this owner captures its view input, refuses if the published libgte state disagrees,
// and only then applies the latched host presentation plan. The widening is applied AFTER the
// retail check, so retail stays the oracle the refusal compares against, and it reaches GTE
// control registers only — the guest's own view descriptor is never written, so no gameplay read
// of it can be affected by the plan.
class ProjectionOwner final {
public:
  using RetailBody = std::function<void(Core &)>;

  void publish(Core &core, const RetailBody &retailBody);

  [[nodiscard]] const ProjectionPublication &previous() const;
  [[nodiscard]] const ProjectionPublication &current() const;

  // Latch the host presentation plan. Until one is latched, publication is identical to retail,
  // which is why this owner changes nothing on a default 4:3 configuration.
  void latchPlan(const GuestProjectionPlan &plan, int32_t distanceScaleNumerator, int32_t distanceScaleDenominator);

  // What the last publication actually put in the GTE: retail when no plan is latched or the
  // plan does not widen.
  [[nodiscard]] const GteProjection &publishedProjection() const;
  [[nodiscard]] bool widenedLastPublication() const;

private:
  // Republish the GTE triple for the latched plan. Called only after the retail comparison has
  // passed, so a disagreement still refuses against retail rather than against the plan.
  void applyPresentationPlan(Core &core, const ProjectionPublication &retail);

  ProjectionPublication previous_{};
  ProjectionPublication current_{};
  GuestProjectionPlan plan_{};
  int32_t distanceScaleNumerator_ = 1;
  int32_t distanceScaleDenominator_ = 1;
  bool planLatched_ = false;
  GteProjection published_{};
  bool widened_ = false;
};

} // namespace ctr
