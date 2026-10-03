#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

namespace ctr {

// The GTE projection triple CTR's own publication leaves are: libgte SetGeomOffset takes the
// horizontal/vertical centre and SetGeomScreen takes the projection distance, and in this
// executable they are three `ctc2` writes to GTE control registers 36, 37 and 26
// (0x8007781C, 0x8007782C; measured on the identity-verified executable).
struct GteProjection {
  int32_t centerX = 0;  // GTE CR36 (OFX) — the horizontal centre
  int32_t centerY = 0;  // GTE CR37 (OFY) — the vertical centre
  int32_t distance = 0; // GTE CR26 (H)  — the projection distance
};

// The pre-GTE view facts the guest hands to its projection publication at 0x80042910. It reads
// width from +0x20, height from +0x22 and H from +0x18, and publishes centre = half the extent.
// Reading them here, from the descriptor, is the "pre-GTE guest state" the owner consumes.
struct GuestViewProjection {
  int32_t width = 0;
  int32_t height = 0;
  int32_t distance = 0;

  [[nodiscard]] bool valid() const {
    return width > 0 && height > 0 && distance > 0;
  }
};

// The widened triple for one host presentation extent.
//
// `H` IS WHAT CARRIES THE HORIZONTAL FIELD (see `CtrWidescreen`), so `distanceScale` is that
// widening factor expressed as a fraction: the caller owns WHY it is that value, and this type stays
// a projection rule with no policy in it.
struct WidenedViewProjection {
  GteProjection retail{};
  GteProjection published{};
  int32_t horizontalScaleNumerator = 1;
  int32_t horizontalScaleDenominator = 1;
  bool widened = false;
};

// Compute the published triple from the guest's own view facts and a host presentation plan.
//
// `plan` must already carry the requested aspect; a 4:3 plan reproduces the retail triple exactly,
// so the shipping default changes nothing.
[[nodiscard]] WidenedViewProjection widenViewProjection(const GuestViewProjection &view,
                                                        const GuestProjectionPlan &plan,
                                                        int32_t distanceScaleNumerator = 1,
                                                        int32_t distanceScaleDenominator = 1);

} // namespace ctr
