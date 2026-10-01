#include "geometry_projection_owner.h"

#include "core.h"
#include "proj_params.h"
#include "projection_owner.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// GTE control-register numbers. psxport's own `libgte_set_geom_*` in proj_params.cpp writes exactly
// these, and the framework does not export the constants, so they are named here rather than written
// inline. `tests/ctr_geometry_projection_owner.cpp` pins them against the framework's own writer with
// a positive AND a negative round trip, so a framework renumbering turns the test red instead of
// silently widening a register nobody reads.
constexpr uint32_t kGteCrOfx = 24;
constexpr uint32_t kGteCrOfy = 25;
constexpr uint32_t kGteCrH = 26;

// The GTE perspective transforms, from the framework's own `gte_name` table in gte_beetle.cpp, which
// is the same table `gte_op_impl` keys on. RTPS is the single-vertex form this image uses (55 static
// sites); RTPT is the three-vertex form it does not, and it is admitted anyway because a widening
// that skipped a second perspective form would be a silent hole in the coverage.
constexpr uint32_t kGteOpRtps = 0x01;
constexpr uint32_t kGteOpRtpt = 0x30;

bool isPerspectiveOp(uint32_t instruction) {
  const uint32_t op = (instruction >> 5) & 0x3Fu;
  return op == kGteOpRtps || op == kGteOpRtpt;
}

} // namespace

ScopedGteProjectionObservation::ScopedGteProjectionObservation(Core &core, CtrGeometryProjectionOwner &owner)
    : owner_(owner), core_(&core) {
  ++owner_.census_.fields;
  // The owner identity travels in the framework's own per-Core observer slot, so there is no
  // process-global registry that could select the wrong instance, and the only thing to undo is this
  // one disarm.
  gte_op_observer_arm(&core, CtrGeometryProjectionOwner::onGteOp, nullptr, &owner_);
}

ScopedGteProjectionObservation::~ScopedGteProjectionObservation() {
  if (core_ != nullptr) {
    gte_preop_observer_disarm(core_);
  }
}

const CtrGeometryProjectionOwner::Census &CtrGeometryProjectionOwner::census() const {
  return census_;
}

void CtrGeometryProjectionOwner::reportCensus() const {
  // Reported the way the projection census is: at the point the run ENDS, because the run that needs
  // the number most is the one that dies. And it always speaks, because the only two possible
  // readings of a silent owner are "no 3D geometry" and "never armed", and this is the number that
  // separates them.
  lucent::info("ctr-geometry-projection",
               "geometry projection owner over {} host field(s): the framework offered {} GTE op(s) "
               "while armed, of which {} were perspective transforms consuming H. Of those {}: {} "
               "widened, {} already carried this owner's widened H, {} left at retail's H, {} were "
               "seen before the plan resolved, {} were refused for want of a plan owner "
               "(sum {} of {}). Retail H at the last widening = {}, published H = {}.",
               census_.fields,
               census_.gteOpsSeen,
               census_.projections,
               census_.projections,
               census_.widenedProjections,
               census_.alreadyOwned,
               census_.leftAtRetail,
               census_.projectionsBeforePlan,
               census_.refusedNoPlanOwner,
               census_.widenedProjections + census_.alreadyOwned + census_.leftAtRetail +
                   census_.projectionsBeforePlan + census_.refusedNoPlanOwner,
               census_.projections,
               census_.lastRetailDistance,
               census_.lastPublishedDistance);
  if (census_.fields > 0 && census_.gteOpsSeen == 0) {
    lucent::warn("ctr-geometry-projection",
                 "the owner was armed for {} field(s) and the framework offered it 0 GTE ops. That is "
                 "'the seam did not fire', NOT 'the guest projected nothing' — the two are "
                 "indistinguishable without this line, and the second reading is the one that would "
                 "have been believed.",
                 census_.fields);
  }
}

CtrGeometryProjectionOwner::RetailTriple CtrGeometryProjectionOwner::readLiveTriple() {
  // H is a plain integer in the GTE (gte.c: `#define H (uint16_t)CR[26]`) and the framework's
  // `libgte_set_geom_screen` writes it unshifted, so it round-trips exactly.
  //
  // OFX/OFY ARE REPORTED AS THE RAW 16.16 WORD AND ARE NOT SCALED, and that is a measurement rather
  // than a simplification. The two guest call sites disagree about this quantity by one bit:
  // PsyQ's own `SetGeomOffset` (0x8007782C) does `sll a0,a0,16` before `ctc2 a0,$24`, while all ten
  // geometry submitters do `sll v1,v1,15`. Both read displacement +0x20, so either they are the same
  // field in two structs or the same field at two scales, and the bytes do not settle which. So this
  // owner REPORTS the centre and never writes it, rather than re-centring through a scale it would
  // have to guess. Re-centring the 3D path is the next measurement, not a line of code.
  return RetailTriple{
      .centerX = static_cast<int32_t>(gte_read_ctrl(kGteCrOfx)),
      .centerY = static_cast<int32_t>(gte_read_ctrl(kGteCrOfy)),
      .distance = gte_read_ctrl(kGteCrH),
  };
}

void CtrGeometryProjectionOwner::onGteOp(
    Core *core, uint64_t ordinal, uint32_t guestPc, uint32_t instruction, void *user) {
  (void)ordinal;
  (void)guestPc;
  if (core == nullptr) {
    return;
  }
  auto *owner = static_cast<CtrGeometryProjectionOwner *>(user);
  if (owner == nullptr) {
    return;
  }
  owner->observe(*core, instruction);
}

void CtrGeometryProjectionOwner::observe(Core &core, uint32_t instruction) {
  ++census_.gteOpsSeen;
  if (!isPerspectiveOp(instruction)) {
    return;
  }
  ++census_.projections;
  const Outcome outcome = applyOwnedPlan(core, readLiveTriple());
  switch (outcome) {
  case Outcome::Widened:
    ++census_.widenedProjections;
    break;
  case Outcome::AlreadyOwned:
    ++census_.alreadyOwned;
    break;
  case Outcome::LeftAtRetail:
    ++census_.leftAtRetail;
    census_.lastRetailDistance = static_cast<int32_t>(readLiveTriple().distance);
    break;
  case Outcome::PlanNotResolved:
    ++census_.projectionsBeforePlan;
    census_.lastRetailDistance = static_cast<int32_t>(readLiveTriple().distance);
    break;
  case Outcome::NoPlanOwner:
    ++census_.refusedNoPlanOwner;
    break;
  }
}

CtrGeometryProjectionOwner::Outcome CtrGeometryProjectionOwner::applyOwnedPlan(Core &core, const RetailTriple &retail) {
  if (projectionOwner_ == nullptr || !projectionOwner_->planLatched()) {
    // Two different situations that both leave retail's values in place, and BOTH are counted rather
    // than merged: a projection seen before the plan resolved is ordinary (the plan resolves on the
    // first descriptor publication, which is later than the first GTE op of a frame), while a
    // projection seen with no plan owner at all is a wiring defect. Reporting them as one number is
    // how "0 of 110,432 widened" gets read as a fact about the title when it is a fact about this
    // file.
    return projectionOwner_ == nullptr ? Outcome::NoPlanOwner : Outcome::PlanNotResolved;
  }

  // IDEMPOTENCE, and it is not an optimisation. The guest publishes the triple ONCE per submission
  // function and then runs many perspective transforms through it, so without this the second vertex
  // would be scaled from the first vertex's already-widened H and the third from that: a 1.34x plan
  // would reach 1.34^n within one draw call. The registers being exactly what this owner last
  // published is the signal that the guest has NOT republished, so the only correct action is none.
  if (retail.distance == lastPublishedDistance_ && publishedOnce_) {
    return Outcome::AlreadyOwned;
  }

  const WidenedViewProjection widened =
      widenViewProjection(GuestViewProjection{.width = projectionOwner_->plan().nativeProjectionExtent.width,
                                              .height = projectionOwner_->plan().nativeProjectionExtent.height,
                                              .distance = static_cast<int32_t>(retail.distance)},
                          projectionOwner_->plan(),
                          projectionOwner_->distanceScaleNumerator(),
                          projectionOwner_->distanceScaleDenominator());
  if (!widened.widened) {
    // A 4:3 plan reproduces retail exactly, and this is the ordinary outcome of a 4:3 run rather than
    // a refusal. Republishing it would be a no-op write; not publishing leaves the guest's own value,
    // which is the same result with less risk.
    publishedOnce_ = false;
    return Outcome::LeftAtRetail;
  }
  // The same refusal `ProjectionOwner` makes, on the same rule: a plan whose projection extent is
  // not wider than the guest's own view would NARROW the picture, and publishing it would squash the
  // 3D scene rather than widen it.
  // ONLY H IS WRITTEN. `widenViewProjection` also re-centres the projection on the widened extent,
  // and applying that half here would be a guess: the guest's two call sites scale the centre by
  // different powers of two (see `readLiveTriple`), so the centring a `<<15` producer needs is not
  // the centring a `<<16` producer needs, and nothing in the recovered bytes says which struct the
  // submitters' +0x20 actually is. Widening H is a pure projection change that adds horizontal field
  // with no convention assumption; re-centring is left where it is already implemented and measured,
  // at the 0x80042910 publication, and the shift this leaves in the 3D path is measured in a real
  // two-leg run rather than designed away.
  libgte_set_geom_screen(&core, widened.published.distance);
  lastPublishedDistance_ = widened.published.distance;
  publishedOnce_ = true;
  // Retail's H for the publication this widened, recorded BEFORE the write overwrote it. Without that
  // ordering the reported "retail H" would be the owner's own value on the next republication, which
  // is the number a reader would take for the guest's.
  census_.lastRetailDistance = static_cast<int32_t>(retail.distance);
  census_.lastPublishedDistance = widened.published.distance;

  // The write is read back, because "I widened it" and "the GTE is holding the widened value" are
  // different claims and only the second one is a rendering result. A mismatch is a fail-loud defect,
  // not a warning: continuing would present a picture that is neither retail's nor the plan's.
  const RetailTriple live = readLiveTriple();
  if (live.distance != widened.published.distance) {
    lucent::error("ctr-geometry-projection",
                  "published H {} did not reach the GTE: read back {}. The transform would then "
                  "consume retail's H while the plan says it is widened, which is the exact failure "
                  "this owner exists to prevent.",
                  widened.published.distance,
                  live.distance);
    std::abort();
  }
  return Outcome::Widened;
}

} // namespace ctr
