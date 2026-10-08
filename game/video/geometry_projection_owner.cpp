#include "geometry_projection_owner.h"

#include "core.h"
#include "proj_params.h"
#include "projection_owner.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// GTE control registers, as written by the framework's `libgte_set_geom_*`.
constexpr uint32_t kGteCrOfx = 24;
constexpr uint32_t kGteCrOfy = 25;
constexpr uint32_t kGteCrH = 26;

// RTPS is the form this image uses; RTPT is admitted so a second perspective form is not a hole.
constexpr uint32_t kGteOpRtps = 0x01;
constexpr uint32_t kGteOpRtpt = 0x30;

bool isPerspectiveOp(uint32_t instruction) {
  const uint32_t op = (instruction >> 5) & 0x3Fu;
  return op == kGteOpRtps || op == kGteOpRtpt;
}

} // namespace

ScopedGteProjectionObservation::ScopedGteProjectionObservation(Core &core, CtrGeometryProjectionOwner &owner)
    : owner_(owner), core_(&core) {
  // The owner travels in the framework's per-Core observer slot.
  gte_op_observer_arm(&core, CtrGeometryProjectionOwner::onGteOp, nullptr, &owner_);
}

ScopedGteProjectionObservation::~ScopedGteProjectionObservation() {
  if (core_ != nullptr) {
    gte_preop_observer_disarm(core_);
  }
}

CtrGeometryProjectionOwner::RetailTriple CtrGeometryProjectionOwner::readLiveTriple() {
  // OFX/OFY stay raw 16.16: PsyQ SetGeomOffset (0x8007782C) shifts by 16 but the ten submitters shift by 15.
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
  if (!isPerspectiveOp(instruction)) {
    return;
  }
  applyOwnedPlan(core, readLiveTriple());
}

CtrGeometryProjectionOwner::Outcome CtrGeometryProjectionOwner::applyOwnedPlan(Core &core, const RetailTriple &retail) {
  if (projectionOwner_ == nullptr || !projectionOwner_->planLatched()) {
    // Before the first descriptor publication is ordinary; no plan owner at all is a wiring defect.
    return projectionOwner_ == nullptr ? Outcome::NoPlanOwner : Outcome::PlanNotResolved;
  }

  // The guest publishes once per submission and transforms many vertices; re-widening would compound.
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
    // A 4:3 plan reproduces retail; leave the guest's value.
    publishedOnce_ = false;
    return Outcome::LeftAtRetail;
  }
  // Only H is written: the centre scale differs between the guest's call sites (see `readLiveTriple`).
  libgte_set_geom_screen(&core, widened.published.distance);
  lastPublishedDistance_ = widened.published.distance;
  publishedOnce_ = true;
  // Read back: a mismatch means the transform would consume retail's H.
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
