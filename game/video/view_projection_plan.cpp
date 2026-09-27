#include "view_projection_plan.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

int32_t widenDistance(int32_t distance, int32_t numerator, int32_t denominator) {
  if (denominator <= 0) {
    lucent::error("ctr-view-projection", "projection scale denominator {} is not positive", denominator);
    std::abort();
  }
  const int64_t scaled = (static_cast<int64_t>(distance) * numerator + denominator / 2) / denominator;
  if (scaled <= 0 || scaled > 0x7FFFFFFF) {
    lucent::error("ctr-view-projection", "scaled projection distance {} is out of range", scaled);
    std::abort();
  }
  return static_cast<int32_t>(scaled);
}

} // namespace

WidenedViewProjection widenViewProjection(const GuestViewProjection &view,
                                          const GuestProjectionPlan &plan,
                                          int32_t distanceScaleNumerator,
                                          int32_t distanceScaleDenominator) {
  if (!view.valid()) {
    lucent::error("ctr-view-projection",
                  "the guest published an unusable view: width={} height={} distance={}",
                  view.width,
                  view.height,
                  view.distance);
    std::abort();
  }
  if (!plan.nativeProjectionExtent.width || !plan.nativeProjectionExtent.height) {
    lucent::error("ctr-view-projection", "the projection plan has no native extent to widen from");
    std::abort();
  }

  WidenedViewProjection result{};
  // Retail's own rule, reproduced from the bytes: the centre is half the guest's extent. It is
  // computed here rather than copied from the caller so the retail baseline and the widened
  // value cannot disagree about what retail is.
  result.retail = GteProjection{
      .centerX = view.width / 2,
      .centerY = view.height / 2,
      .distance = view.distance,
  };
  result.horizontalScaleNumerator = distanceScaleNumerator;
  result.horizontalScaleDenominator = distanceScaleDenominator;
  result.published = result.retail;

  const bool widenExtent = plan.projectionExtent.width > plan.nativeProjectionExtent.width;
  if (!widenExtent || distanceScaleNumerator == distanceScaleDenominator) {
    return result;
  }

  // Re-centre on the widened extent so the extra field is symmetric about the same centre, and
  // scale H so the extra field is actually rendered rather than stretched into the same pixels.
  result.published.centerX = plan.projectionCenterX;
  result.published.centerY = plan.nativeProjectionExtent.height / 2;
  result.published.distance = widenDistance(view.distance, distanceScaleNumerator, distanceScaleDenominator);
  result.widened = true;
  return result;
}

} // namespace ctr
