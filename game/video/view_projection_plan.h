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
// WHY H IS LEFT ALONE. The GTE maps a world point to sx = H*X/Z + OFX, so the horizontal FIELD
// is fixed by H and the horizontal EXTENT only decides how many pixels that field is spread
// across. Widening the extent alone would therefore stretch the picture, which is banned.
// Widening H is the only way to add horizontal field, and the measured reason it is safe in
// this title is on `CtrWidescreen`: CTR's geometry cull compares object Z against a scratchpad
// near plane that is a literal 0 or 2 (0x8006E5D0, 0x8006F04C), NOT against H, so raising H
// cannot cull near geometry the way it could in a title that uses H as the bound.
struct WidenedViewProjection {
  GteProjection retail{};
  GteProjection published{};
  int32_t horizontalScaleNumerator = 1;
  int32_t horizontalScaleDenominator = 1;
  bool widened = false;
};

// Compute the published triple from the guest's own view facts and a host presentation plan.
//
// `plan` must already carry the requested aspect; a 4:3 plan reproduces the retail triple
// exactly, so the shipping default changes nothing. `distanceScale` is the widening factor
// applied to H, expressed as a fraction so the caller owns WHY it is that value and this
// function stays a projection rule with no policy in it.
[[nodiscard]] WidenedViewProjection widenViewProjection(const GuestViewProjection &view,
                                                        const GuestProjectionPlan &plan,
                                                        int32_t distanceScaleNumerator = 1,
                                                        int32_t distanceScaleDenominator = 1);

} // namespace ctr
