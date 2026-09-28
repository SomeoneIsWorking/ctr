#pragma once

#include "guest_widescreen_projection.h"
#include "view_projection_plan.h"

#include <cstdint>

class Core;

namespace ctr {

class ProjectionOwner;

// Owns CTR's ONE widening decision, in two halves that are deliberately the same object because they
// are the same decision seen from two ends:
//
//   1. `presentationAspect` is the title's ANSWER to the framework's question, and it is the only
//      thing standing between the configured aspect and a plan. Before this existed
//      `CtrRuntime::guestWidescreenProjection()` returned the base nullptr, so
//      `gpu_vk_latch_guest_projection` always resolved `requested = Standard4x3` and every plan was
//      4:3 regardless of the settings file. That is the measured root cause of CTR not widening,
//      and it is why this is a class rather than a line in the frame driver: a null answer cannot be
//      widened by anything downstream.
//   2. `latch` publishes the resolved plan into the projection owner, so the GTE triple actually
//      widens instead of being computed and then dropped.
//
// WHY WIDENING `H` IS SAFE IN THIS TITLE, restated because THE PREVIOUS VERSION OF THIS PARAGRAPH WAS
// WRONG AND THE CORRECTION CHANGES WHAT MAY BE DONE NEXT.
//
// The claim was: CTR's geometry cull compares object Z against a scratchpad near plane that is a
// literal 0 or 2, not against `H`, "and nothing in the 128,512-word text reads `H` back".
//
// The first half is right and is still measured (`tools/ctr_binary_probe.py near-plane`): the two
// geometry paths store a literal 0 or 2 at `DAT_1F800054`, chosen by the caller's third argument
// (`0x8006E5C0 addiu v1,a2,-2` / `bgtz`, stored at `0x8006E5D0`, repeated at `0x8006F04C`). So
// raising `H` does not cull near geometry the way it would in a title that uses `H` as its bound.
//
// The second half is FALSE. `tools/ctr_binary_probe.py gte-projection` finds **4** `cfc2 rX,$26`
// sites that read `H` back out of the GTE: 0x8006A6B8, 0x80070AF8, 0x80070EE8 and 0x80071150. The
// guest stores `2H - CR7` into its scene descriptor at +0xF4, DIVIDES by `H` at 0x80070AF8, builds
// `4H` at 0x80070EE8, and uses `H/2` for a vertex at 0x80071150. `H` in this title is therefore a
// COUPLED scalar, not a pure projection parameter, and "widening `H` is safe because nothing reads
// it" is void here — which is exactly the trap the workspace map warns about for every other title.
//
// CONSEQUENCE, and it is the reason the widening is still incomplete rather than merely cautious: the
// widening reaches 3D geometry and the guest's own viewport math moves with it, so the picture widens
// by much less than the focal-length ratio. Measured: 911 of 110,553 perspective transforms widened
// (16:9) against 0 of 110,432 (4:3), the rightmost submitted x moved 812 -> 894, and the drawn band
// 695 -> 719 of 960 columns with a 241-column margin that is 0/173,520 non-black and belongs to 2D
// content a projection change cannot reach. See docs/issues/0031 and `CtrGeometryProjectionOwner`.
//
// WHAT IT NEVER DOES: write a guest byte. The plan reaches the GTE control registers only, through
// the projection owner's existing `libgte_set_geom_*` path, and the guest's view descriptor keeps its
// retail values.
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
