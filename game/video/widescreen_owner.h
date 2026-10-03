#pragma once

#include "guest_widescreen_projection.h"
#include "view_projection_plan.h"

#include <cstdint>

class Core;

namespace ctr {

class ProjectionOwner;

// Owns CTR's ONE widening decision, in two halves that are the same decision seen from two ends:
// `presentationAspect` answers the framework's question and is the only thing standing between the
// configured aspect and a plan, and `planFor` resolves the plan from the extent the GUEST's own
// publication just carried. A null aspect answer cannot be widened by anything downstream, so the
// two halves are one object.
//
// WIDENING REACHES `H` (GTE control register 26), never the horizontal extent alone: the GTE maps a
// point to sx = H*X/Z + OFX, so widening the extent by itself would stretch the picture. `H` is a
// COUPLED scalar here — four measured `cfc2 rX,$26` sites read it back out of the GTE and derive
// `2H`, `4H` and `H/2` — so the widened picture is narrower than the focal-length ratio and the
// guest's own viewport math moves with it. This owner writes GTE control registers only; the guest's
// view descriptor keeps its retail values.
class CtrWidescreen final : public GuestWidescreenProjection {
public:
  struct LatchResult {
    bool widens = false;
    GuestProjectionPlan plan{};
  };

  PresentationAspect presentationAspect(const Core &core) const override;

  // The plan for the extent THIS publication carried. Called by the projection owner on every
  // publication, so the native extent is always the guest's own and never a stale display register.
  GuestProjectionPlan planFor(Core &core, const GuestViewProjection &view);

  // Has the plan been resolved yet? It is resolved on the first publication, not at boot.
  [[nodiscard]] bool latched() const;

  [[nodiscard]] const GuestProjectionPlan &plan() const;
  [[nodiscard]] int32_t distanceScaleNumerator() const;
  [[nodiscard]] int32_t distanceScaleDenominator() const;
  void report(const GuestViewProjection &view) const;

  static const char *aspectName(PresentationAspect aspect);

private:
  GuestProjectionPlan plan_{};
  int32_t distanceScaleNumerator_ = 1;
  int32_t distanceScaleDenominator_ = 1;
  bool latched_ = false;
};

} // namespace ctr
