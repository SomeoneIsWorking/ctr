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

// Captures the view input of the retail projection publication, refuses if the published libgte
// state disagrees, then applies the latched plan to GTE control registers only.
class ProjectionOwner final {
public:
  using RetailBody = std::function<void(Core &)>;

  // Identified by the return address that reached the owner.
  enum class Source : uint8_t {
    LensFlare = 0, // 0x80024CCC, inside the lens-flare producer [0x80024C4C,0x80025138)
    StateZero = 1, // 0x8003BD2C
    Overlay = 2,   // 0x8003F5C0
  };
  [[nodiscard]] static uint32_t sourceReturnAddress(Source source);

  void publish(Core &core, const RetailBody &retailBody, Source source);

  [[nodiscard]] const ProjectionPublication &previous() const;
  [[nodiscard]] const ProjectionPublication &current() const;

  // The plan is derived from the guest's own view on every publication: the boot display mode is not
  // the projected extent (512 wide, OFX = width/2).
  using PlanSource = std::function<GuestProjectionPlan(Core &, const GuestViewProjection &)>;
  void setPlanSource(PlanSource source);

  // A fixed plan overrides any plan source.
  void latchPlan(const GuestProjectionPlan &plan, int32_t distanceScaleNumerator, int32_t distanceScaleDenominator);

  // What the last publication actually put in the GTE: retail when no plan is latched or the
  // plan does not widen.
  [[nodiscard]] const GteProjection &publishedProjection() const;
  [[nodiscard]] bool widenedLastPublication() const;

  // Read-only for `CtrGeometryProjectionOwner`, so the plan has one home.
  [[nodiscard]] bool planLatched() const {
    return planLatched_;
  }
  [[nodiscard]] const GuestProjectionPlan &plan() const {
    return plan_;
  }
  [[nodiscard]] int32_t distanceScaleNumerator() const {
    return distanceScaleNumerator_;
  }
  [[nodiscard]] int32_t distanceScaleDenominator() const {
    return distanceScaleDenominator_;
  }

private:
  // Runs only after the retail comparison passed.
  void applyPresentationPlan(Core &core, const ProjectionPublication &retail);

  ProjectionPublication previous_{};
  ProjectionPublication current_{};
  GuestProjectionPlan plan_{};
  int32_t distanceScaleNumerator_ = 1;
  int32_t distanceScaleDenominator_ = 1;
  bool planLatched_ = false;
  PlanSource planSource_{};
  GteProjection published_{};
  bool widened_ = false;
};

} // namespace ctr
