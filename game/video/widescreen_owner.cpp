#include "widescreen_owner.h"

#include "core.h"
#include "game.h"
#include "gpu_vk.h"
#include "guest_widescreen_projection.h"
#include "mods.h"
#include "projection_owner.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// The native extent comes from the guest's own view (GP1(08) mode 0x08000002 is 512 dots; OFX = width/2 at 0x8004293C),
// never a constant.
} // namespace

GuestProjectionPlan CtrWidescreen::planFor(Core &core, const GuestViewProjection &view) {
  if (!view.valid()) {
    lucent::error("wide",
                  "CTR widescreen cannot widen an unusable guest view: {}x{} H={}",
                  view.width,
                  view.height,
                  view.distance);
    std::abort();
  }
  // Driven by the extent the guest's own publication carried (view descriptor +0x20/+0x22), not a display register.
  const GuestProjectionPlan plan = gpu_vk_latch_guest_projection(&core,
                                                                 GuestProjectionGeometry{
                                                                     .extent = {view.width, view.height},
                                                                     .drawWidth = view.width,
                                                                 });
  if (!plan_.nativeProjectionExtent.width) {
    plan_ = plan;
    distanceScaleNumerator_ = plan.projectionExtent.width;
    distanceScaleDenominator_ = plan.nativeProjectionExtent.width;
    latched_ = true;
    report(view);
  }
  return plan;
}

void CtrWidescreen::report(const GuestViewProjection &view) const {
  lucent::info("wide",
               "CTR widescreen owner resolved from the GUEST'S OWN published view ({}x{}, the view "
               "descriptor's own +0x20/+0x22): aspect={} native presentation {}x{} -> presentation "
               "{}x{} (margin {}px), native projection {}x{} -> projection {}x{} (margin {}px, centre "
               "X {}), H scale {}/{}, guest draw width {} -> {}. {}",
               view.width,
               view.height,
               aspectName(plan_.aspect),
               plan_.nativeExtent.width,
               plan_.nativeExtent.height,
               plan_.presentationExtent.width,
               plan_.presentationExtent.height,
               plan_.presentationHorizontalMargin,
               plan_.nativeProjectionExtent.width,
               plan_.nativeProjectionExtent.height,
               plan_.projectionExtent.width,
               plan_.projectionExtent.height,
               plan_.projectionHorizontalMargin,
               plan_.projectionCenterX,
               distanceScaleNumerator_,
               distanceScaleDenominator_,
               plan_.nativeGuestDrawWidth,
               plan_.guestDrawWidth,
               plan_.widescreen() ? "This plan WIDENS the projection."
                                  : "This plan does NOT widen: the published triple is retail's own.");
}

bool CtrWidescreen::latched() const {
  return latched_;
}

const char *CtrWidescreen::aspectName(PresentationAspect aspect) {
  switch (aspect) {
  case PresentationAspect::Standard4x3:
    return "4:3";
  case PresentationAspect::Wide16x9:
    return "16:9";
  case PresentationAspect::UltraWide21x9:
    return "21:9";
  case PresentationAspect::MatchSink:
    return "match-sink";
  }
  return "?";
}

} // namespace ctr
