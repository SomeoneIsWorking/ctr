#pragma once

#include "view_projection_plan.h"

#include <cstdint>

class Core;

namespace ctr {

class ProjectionOwner;

// CTR's SECOND projection application point, and it exists because the first one was measured to
// cover 1 of 18 sites.
//
// WHAT WAS MEASURED, and the arithmetic is the whole argument. Every count below is re-derivable
// from the identity-verified executable:
//
//   * the guest writes the GTE projection distance H (control register 26) from 16 raw `ctc2 rX,$26`
//     words plus 2 `jal SetGeomScreen` calls — 18 writers in 128,512 words;
//   * `ProjectionOwner` listens at 0x80042910, reached by 2 of those calls plus the state-zero
//     literal. It is a correct owner of that publication and it widens all 176 of them;
//   * TEN of the raw writers are INTERIOR LABELS inside the guest's geometry-submission functions
//     (0x80069FFC, 0x8006AAA8, 0x8006DC30, 0x8006E26C, 0x8006E588, 0x8006F004, 0x8006F9A8,
//     0x8006FE70, 0x80070388, 0x80070950). Each reads the SAME view descriptor (+0x18 distance,
//     +0x20 OFX, +0x22 OFY) and republishes the triple itself, immediately before its own GTE
//     perspective transforms. All ten carry the same five-instruction tail verbatim.
//
// So a widening applied only at 0x80042910 is overwritten by the guest before any 3D geometry is
// projected. That is why the canvas widened (512 -> 684) and the picture did not.
//
// WHY THE SEAM IS THE PROJECTION OP AND NOT THE TEN FUNCTION ENTRIES. Each `ctc2 rX,$26` is an
// interior label inside a function body, and a label reached by straight-line fall-through can never
// be an override key: the override would return to an `r31` the tail never set. The enclosing
// entries ARE valid keys (each has >= 1 direct `jal` caller and 0 `j` targets), but an entry-level
// override cannot help either, because the guest writes CR24/25/26 *inside* the body and then
// transforms — so by the time an override regains control the widened value is already gone.
//
// The framework's per-Core GTE op observer fires immediately BEFORE `GTE_Instruction` for every guest
// COP2 op (lightrec_executor.cpp `cop2Operation` -> `gte_op_at` -> `GtePreOpObserver::observeAround`),
// and Lightrec exports the GTE registers back after the op, so a value written in the pre-op callback
// is the value the transform actually consumes. That is the measured instant at which the triple is
// used, and keying on it covers all 18 writers by construction instead of by an enumerated list a new
// submitter could slip past. The owner identity travels in the framework's own per-Core observer
// `void*`; there is no process-global registry and no way for one Core's owner to answer for another.
//
// WHAT IT DOES NOT DO. It writes GTE control registers only, through the framework's own
// `libgte_set_geom_*` path; no guest byte is touched. Retail's triple is re-read from the live
// registers on every projection rather than accumulated, so a 4:3 run republishes retail's values
// exactly and the owner is inert.
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

  // THE CENSUS, and the reason it exists. This owner only ever speaks when it changes something, so
  // "0 projections" and "the observer never fired" would otherwise be the same silence. `gteOpsSeen`
  // is the denominator: every GTE op the framework offered while the owner was armed, whether or not
  // it was a perspective transform. A run in which the observer never fired reports 0 of 0 FIELDS,
  // which cannot be read as "widened nothing".
  struct Census {
    uint64_t fields = 0;      // host fields this owner was armed for
    uint64_t gteOpsSeen = 0;  // GTE ops offered while armed — the denominator
    uint64_t projections = 0; // of those, the perspective transforms that consume H
    uint64_t widenedProjections = 0;
    // Every remaining outcome, kept apart because they mean different things, and together EXHAUSTIVE
    // so the five sum to `projections`. An earlier revision reported only "not widened", which made
    // 911 of 110,553 read as an unexplained remainder rather than as 109,642 idempotence skips.
    uint64_t alreadyOwned = 0;          // the registers already held this owner's widened H
    uint64_t leftAtRetail = 0;          // the plan is 4:3, or does not widen this view
    uint64_t projectionsBeforePlan = 0; // seen before the first descriptor publication resolved it
    uint64_t refusedNoPlanOwner = 0;    // no plan owner was bound: a WIRING defect, not a title fact
    int32_t lastRetailDistance = 0;
    int32_t lastPublishedDistance = 0;
  };

  // A field the owner was live for is counted by the arming scope, not by a call the driver has to
  // remember: `ProjectionOwner` has an `endField()` that nothing calls, so this owner deliberately
  // has no lifecycle method that can be forgotten. `ScopedGteProjectionObservation` is the only way
  // to arm, and it counts the field on both paths.

  [[nodiscard]] const Census &census() const;
  void reportCensus() const;

  // The plan this owner applies. It is READ from the projection owner rather than latched here, so
  // the resolved plan and its scale have exactly one home; a second copy would be a second answer to
  // one question and could disagree with the first owner's without either noticing.
  void setProjectionOwner(const ProjectionOwner *owner) {
    projectionOwner_ = owner;
  }

  // The framework's per-Core GTE pre-op callback. Public because the observer is a plain function
  // pointer; it is not a title entry point and is named so it reads as one. `user` is the owner.
  static void onGteOp(Core *core, uint64_t ordinal, uint32_t guestPc, uint32_t instruction, void *user);

  // The live triple, read from the GTE control registers the framework names in proj_params.h.
  [[nodiscard]] static RetailTriple readLiveTriple();

  // What one perspective transform did with the triple it found. Exhaustive on purpose: a boolean
  // return made "already owned this frame's widened H" indistinguishable from "declined to widen", and
  // those are the two numbers a reader most needs apart.
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
// exit.
//
// The two halves are bound to the same lifetime on purpose, and the reason is the same one
// `FieldOverrideScope` gives: an observer left armed past the field would be answering for a field
// the driver no longer owns, and one disarmed early would let a translated block reach the GTE
// unwatched. A method pair the caller must remember is exactly what went wrong next door, so this
// type exists instead of a pair.
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
