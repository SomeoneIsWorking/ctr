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

// CTR's native projection extent is NOT a constant here, and writing one down is what broke the
// first version of this owner. Measured on the provisioned image and confirmed at runtime:
//
//   * the guest's GP1(08) display mode is 0x08000002, and `gp1_display_width` decodes
//     `mode & 3 == 2` as 512 dots — so `s_disp_w` is 512, not the 320 one assumes for NTSC;
//   * the boot literal publication is OFX=256, and the measured rule is OFX = view width / 2
//     (0x8004293C `sra a0,a0,1`), so the guest's own view descriptor is 512 wide as well.
//
// Both agree, which is why the 4:3 and 16:9 legs were BYTE-IDENTICAL when the owner widened from a
// guessed 320 baseline: the plan's 428-wide projection extent is NARROWER than the real 512 native
// extent, so `present_display_width` correctly declined to call that a widening and the presenter
// kept 512. The extent is therefore read from the guest's own state at the moment it is known, and
// the latch refuses to run before the guest has published a display mode at all.

} // namespace

PresentationAspect CtrWidescreen::presentationAspect(const Core &core) const {
  if (!core.game) {
    return PresentationAspect::Standard4x3;
  }
  switch (core.game->mods.aspect) {
  case ASPECT_4_3:
    return PresentationAspect::Standard4x3;
  case ASPECT_16_9:
    return PresentationAspect::Wide16x9;
  case ASPECT_21_9:
    return PresentationAspect::UltraWide21x9;
  case ASPECT_AUTO:
    return PresentationAspect::MatchSink;
  default:
    lucent::error("wide", "CTR received invalid aspect selector {}", core.game->mods.aspect);
    std::abort();
  }
}

GuestProjectionPlan CtrWidescreen::planFor(Core &core, const GuestViewProjection &view) {
  if (!view.valid()) {
    lucent::error("wide",
                  "CTR widescreen cannot widen an unusable guest view: {}x{} H={}",
                  view.width,
                  view.height,
                  view.distance);
    std::abort();
  }
  // The framework's ONE implementation of "resolve the plan from configuration and the live sink",
  // driven by the extent the GUEST's own publication just carried: its view descriptor's width at
  // +0x20 and height at +0x22, the same two words `0x80042910` reads. A display-mode register would
  // read 320 against a 512-dot picture, and a plan built on it is narrower than what it widens.
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
