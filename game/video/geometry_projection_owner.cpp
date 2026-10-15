#include "video/geometry_projection_owner.h"

#include "core.h"
#include "proj_params.h"
#include "video/projection_owner.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// GTE control registers, as written by the framework's `psx::gte::setGeom*`.
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

ScopedGteProjectionObservation::ScopedGteProjectionObservation(Core &core, GeometryProjectionOwner &owner)
    : owner_(owner), core_(&core) {
  // The owner travels in the framework's per-Core observer slot.
  core.gte().observer().arm({.pre = GeometryProjectionOwner::onGteOp}, &owner_);
}

ScopedGteProjectionObservation::~ScopedGteProjectionObservation() {
  if (core_ != nullptr) {
    (void)core_->gte().observer().disarm();
  }
}

GeometryProjectionOwner::RetailTriple GeometryProjectionOwner::readLiveTriple(Core &core) {
  // OFX/OFY stay raw 16.16: PsyQ SetGeomOffset (0x8007782C) shifts by 16 but the ten submitters shift by 15.
  return RetailTriple{
      .centerX = static_cast<int32_t>(core.gte().readControl(kGteCrOfx)),
      .centerY = static_cast<int32_t>(core.gte().readControl(kGteCrOfy)),
      .distance = core.gte().readControl(kGteCrH),
  };
}

void GeometryProjectionOwner::onGteOp(Core *core, const GteOp &op, void *user) {
  if (core == nullptr) {
    return;
  }
  auto *owner = static_cast<GeometryProjectionOwner *>(user);
  if (owner == nullptr) {
    return;
  }
  owner->observe(*core, op.instruction);
}

void GeometryProjectionOwner::observe(Core &core, uint32_t instruction) {
  if (!isPerspectiveOp(instruction)) {
    return;
  }
  applyOwnedPlan(core, readLiveTriple(core));
}

GeometryProjectionOwner::Outcome GeometryProjectionOwner::applyOwnedPlan(Core &core, const RetailTriple &retail) {
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
  psx::gte::setGeomScreen(core, widened.published.distance);
  lastPublishedDistance_ = widened.published.distance;
  publishedOnce_ = true;
  // Read back: a mismatch means the transform would consume retail's H.
  const RetailTriple live = readLiveTriple(core);
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
