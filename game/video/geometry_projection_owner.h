#pragma once

#include "view_projection_plan.h"

#include <cstdint>

class Core;

namespace ctr {

class ProjectionOwner;

// CTR's SECOND projection application point, and it exists because the first covers 1 of 18 writers
// of the GTE projection distance: the guest writes H (control register 26) from sixteen raw `ctc2`
// words plus two `jal SetGeomScreen` calls, and `ProjectionOwner` listens at only the descriptor
// publication (0x80042910). Ten of the raw writers are interior labels inside the geometry
// submitters, each reading the same view descriptor and republishing the triple itself before its own
// transforms.
//
// THE SEAM IS THE PROJECTION OP, NOT THOSE LABELS. An interior label is never an override key — the
// override would return to an `r31` the tail never set — and an entry-level override cannot help
// either, because the guest writes CR24/25/26 inside the body and then transforms. The framework's
// per-Core GTE pre-op observer fires immediately before every guest COP2 op and Lightrec exports the
// registers afterwards, so a value written there is the one the transform consumes. Keying on that
// instant covers all 18 writers by construction rather than by a list a new submitter could slip
// past. The owner identity travels in the framework's own per-Core observer `void*`; there is no
// process-global registry and no way for one Core's owner to answer for another.
//
// It writes GTE control registers only, through the framework's `libgte_set_geom_*` path, and retail's
// triple is re-read from the live registers on every projection, so a 4:3 run republishes retail's
// values exactly and the owner is inert.
class CtrGeometryProjectionOwner final {
public:
  // A retired GTE op, as it stands at the moment the perspective transform is about to consume it.
  // The centres are the RAW 16.16 control-register words, NOT screen offsets: the guest's two
  // publication sites scale this quantity by different powers of two, so there is no scale this
  // struct can carry that would be right for both. See `readLiveTriple`.
  struct RetailTriple {
    int32_t centerX = 0;
    int32_t centerY = 0;
    uint32_t distance = 0;
  };

  // The census, and the reason it exists: this owner speaks only when it changes something, so "0
  // projections" and "the observer never fired" would otherwise be one silence. `gteOpsSeen` is the
  // denominator — every GTE op the framework offered while the owner was armed, whether or not it was
  // a perspective transform — so a run in which the observer never fired reports 0 of 0 FIELDS,
  // which cannot be read as "widened nothing".
  struct Census {
    uint64_t fields = 0;      // host fields this owner was armed for
    uint64_t gteOpsSeen = 0;  // GTE ops offered while armed — the denominator
    uint64_t projections = 0; // of those, the perspective transforms that consume H
    uint64_t widenedProjections = 0;
    // Every remaining outcome, kept apart because they mean different things and together are
    // EXHAUSTIVE, so the five sum to `projections`.
    uint64_t alreadyOwned = 0;          // the registers already held this owner's widened H
    uint64_t leftAtRetail = 0;          // the plan is 4:3, or does not widen this view
    uint64_t projectionsBeforePlan = 0; // seen before the first descriptor publication resolved it
    uint64_t refusedNoPlanOwner = 0;    // no plan owner was bound: a WIRING defect, not a title fact
    int32_t lastRetailDistance = 0;
    int32_t lastPublishedDistance = 0;
  };

  // A field the owner was live for is counted by the arming scope, not by a call the driver has to
  // remember: `ScopedGteProjectionObservation` is the only way to arm, and it counts the field on both
  // paths.

  [[nodiscard]] const Census &census() const;
  void reportCensus() const;

  // The plan this owner applies. It is READ from the projection owner rather than latched here, so
  // the resolved plan and its scale have exactly one home; a second copy could disagree with the
  // first owner's without either noticing.
  void setProjectionOwner(const ProjectionOwner *owner) {
    projectionOwner_ = owner;
  }

  // The framework's per-Core GTE pre-op callback. Public because the observer is a plain function
  // pointer; it is not a title entry point and is named so it reads as one. `user` is the owner.
  static void onGteOp(Core *core, uint64_t ordinal, uint32_t guestPc, uint32_t instruction, void *user);

  // The live triple, read from the GTE control registers the framework names in proj_params.h.
  [[nodiscard]] static RetailTriple readLiveTriple();

  // What one perspective transform did with the triple it found. Exhaustive on purpose: a boolean
  // return made "already owned this frame's widened H" indistinguishable from "declined to widen".
  enum class Outcome : uint8_t {
    Widened,         // a guest republication was widened
    AlreadyOwned,    // the registers already held this owner's published H; nothing to do
    LeftAtRetail,    // the plan does not widen this view (a 4:3 run)
    PlanNotResolved, // seen before the first descriptor publication resolved the plan
    NoPlanOwner,     // no plan owner was bound: a wiring defect, reported rather than absorbed
  };

  // Apply the owned plan to a retired triple.
  // Exposed for the focused test, which drives it directly rather than waiting for the GTE.
  Outcome applyOwnedPlan(Core &core, const RetailTriple &retail);

  // One GTE op offered by the framework. Public only because the observer is a plain function
  // pointer; the census split stays here so the callback itself is a forward.
  void observe(Core &core, uint32_t instruction);

private:
  friend class ScopedGteProjectionObservation;

  const ProjectionOwner *projectionOwner_ = nullptr;
  Census census_{};
  // The H this owner last published, and whether it published anything. That is what makes the
  // widening idempotent across the many perspective transforms one submission function runs through a
  // single published triple; see `applyOwnedPlan`.
  int32_t lastPublishedDistance_ = 0;
  bool publishedOnce_ = false;
};

// Arms the framework's per-Core GTE op observer for exactly one host field and disarms it on scope
// exit. Both halves are bound to the same lifetime: an observer left armed past the field would
// answer for a field the driver no longer owns, and one disarmed early would let a translated block
// reach the GTE unwatched.
class ScopedGteProjectionObservation final {
public:
  ScopedGteProjectionObservation(Core &core, CtrGeometryProjectionOwner &owner);
  ~ScopedGteProjectionObservation();

  ScopedGteProjectionObservation(const ScopedGteProjectionObservation &) = delete;
  ScopedGteProjectionObservation &operator=(const ScopedGteProjectionObservation &) = delete;

private:
  CtrGeometryProjectionOwner &owner_;
  Core *core_ = nullptr;
};

} // namespace ctr
