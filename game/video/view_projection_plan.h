#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

namespace ctr {

// The GTE projection triple: SetGeomOffset/SetGeomScreen are ctc2 writes to control registers
// 36, 37 and 26 (0x8007781C, 0x8007782C).
struct GteProjection {
  int32_t centerX = 0;  // GTE CR36 (OFX) — the horizontal centre
  int32_t centerY = 0;  // GTE CR37 (OFY) — the vertical centre
  int32_t distance = 0; // GTE CR26 (H)  — the projection distance
};

// The view facts the guest hands to its publication at 0x80042910: width +0x20, height +0x22, H +0x18.
struct GuestViewProjection {
  int32_t width = 0;
  int32_t height = 0;
  int32_t distance = 0;

  [[nodiscard]] bool valid() const {
    return width > 0 && height > 0 && distance > 0;
  }
};

// The widened triple; the distance ratio is the H widening factor.
struct WidenedViewProjection {
  GteProjection retail{};
  GteProjection published{};
  int32_t horizontalScaleNumerator = 1;
  int32_t horizontalScaleDenominator = 1;
  bool widened = false;
};

// A 4:3 plan reproduces the retail triple exactly.
[[nodiscard]] WidenedViewProjection widenViewProjection(const GuestViewProjection &view,
                                                        const GuestProjectionPlan &plan,
                                                        int32_t distanceScaleNumerator = 1,
                                                        int32_t distanceScaleDenominator = 1);

} // namespace ctr
