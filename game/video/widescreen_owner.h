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
// WHY WIDENING `H` IS SAFE IN THIS TITLE, and this is the load-bearing measurement rather than an
// assumption carried over from a sibling. Crash 1 uses `H` as its GTE NEAR PLANE, so raising it there
// culls near geometry; Vagrant Story branches on its projection distance, so raising it flips a
// gameplay decision. CTR does NEITHER: its two geometry paths compare object Z against a scratchpad
// near plane that is a LITERAL 0 or 2 chosen by the caller's third argument (`0x8006E5C0 addiu
// v1,a2,-2` / `bgtz`, stored to `DAT_1F800054` at `0x8006E5D0`, repeated at `0x8006F04C`), and nothing in
// the 128,512-word text reads `H` back. So raising `H` adds horizontal field without culling.
// (docs/issues/0026.)
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
