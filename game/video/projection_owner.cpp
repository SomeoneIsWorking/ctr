#include "projection_owner.h"

#include "core.h"
#include "native_ownership.h"
#include "proj_params.h"

#include <cmath>
#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

constexpr uint32_t kScreenDistanceOffset = 0x18u;
constexpr uint32_t kWidthOffset = 0x20u;
constexpr uint32_t kHeightOffset = 0x22u;

int32_t signedHalf(uint16_t value) {
  return static_cast<int16_t>(value) / 2;
}

bool agrees(float actual, int32_t expected) {
  return std::fabs(actual - static_cast<float>(expected)) < 0.001f;
}

} // namespace

void ProjectionOwner::publish(Core &core, const RetailBody &retailBody, Source source) {
  if (!retailBody) {
    lucent::error("ctr-projection", "projection publication requires the preserved retail body");
    std::abort();
  }

  ProjectionPublication next{
      .source = core.r[4],
      .nativeWidth = static_cast<int16_t>(core.mem_r16(core.r[4] + kWidthOffset)),
      .nativeHeight = static_cast<int16_t>(core.mem_r16(core.r[4] + kHeightOffset)),
      .centerX = signedHalf(core.mem_r16(core.r[4] + kWidthOffset)),
      .centerY = signedHalf(core.mem_r16(core.r[4] + kHeightOffset)),
      .screenDistance = core.mem_r32(core.r[4] + kScreenDistanceOffset),
      .sequence = current_.sequence + 1u,
  };

  retailBody(core);

  const auto &published = core.rsub.projParams;
  if (!published.geomValid() || !agrees(published.geomOfx(), next.centerX) ||
      !agrees(published.geomOfy(), next.centerY) || !agrees(published.geomH(), next.screenDistance)) {
    lucent::error("ctr-projection",
                  "retail projection publication disagreed with view 0x{:08X}: expected ({},{},H={}), got "
                  "({},{},H={})",
                  next.source,
                  next.centerX,
                  next.centerY,
                  next.screenDistance,
                  published.geomOfx(),
                  published.geomOfy(),
                  published.geomH());
    std::abort();
  }

  previous_ = current_;
  current_ = next;
  // A fixed `latchPlan` wins over the callback.
  if (planSource_ && !planLatched_) {
    const GuestProjectionPlan resolved =
        planSource_(core,
                    GuestViewProjection{.width = next.nativeWidth,
                                        .height = next.nativeHeight,
                                        .distance = static_cast<int32_t>(next.screenDistance)});
    plan_ = resolved;
    // H scales by the plan's widening factor so the extra field is rendered, not stretched.
    distanceScaleNumerator_ = resolved.projectionExtent.width;
    distanceScaleDenominator_ = resolved.nativeProjectionExtent.width;
    planLatched_ = true;
  }
  applyPresentationPlan(core, next);
}

void ProjectionOwner::setPlanSource(PlanSource source) {
  planSource_ = std::move(source);
}

void ProjectionOwner::latchPlan(const GuestProjectionPlan &plan,
                                int32_t distanceScaleNumerator,
                                int32_t distanceScaleDenominator) {
  if (distanceScaleNumerator <= 0 || distanceScaleDenominator <= 0) {
    lucent::error("ctr-projection",
                  "projection scale {}/{} is not a positive ratio",
                  distanceScaleNumerator,
                  distanceScaleDenominator);
    std::abort();
  }
  plan_ = plan;
  distanceScaleNumerator_ = distanceScaleNumerator;
  distanceScaleDenominator_ = distanceScaleDenominator;
  planLatched_ = true;
}

const GteProjection &ProjectionOwner::publishedProjection() const {
  return published_;
}

bool ProjectionOwner::widenedLastPublication() const {
  return widened_;
}

uint32_t ProjectionOwner::sourceReturnAddress(Source source) {
  switch (source) {
  case Source::LensFlare:
    return native::kProjectionReturnLensflare;
  case Source::StateZero:
    return native::kProjectionReturnState;
  case Source::Overlay:
    return native::kProjectionReturnOverlay;
  }
  return 0u;
}

const ProjectionPublication &ProjectionOwner::previous() const {
  return previous_;
}

const ProjectionPublication &ProjectionOwner::current() const {
  return current_;
}

void ProjectionOwner::applyPresentationPlan(Core &core, const ProjectionPublication &retail) {
  published_ = GteProjection{
      .centerX = retail.centerX,
      .centerY = retail.centerY,
      .distance = static_cast<int32_t>(retail.screenDistance),
  };
  widened_ = false;
  if (!planLatched_) {
    return;
  }
  const WidenedViewProjection widened =
      widenViewProjection(GuestViewProjection{.width = retail.nativeWidth,
                                              .height = retail.nativeHeight,
                                              .distance = static_cast<int32_t>(retail.screenDistance)},
                          plan_,
                          distanceScaleNumerator_,
                          distanceScaleDenominator_);
  if (!widened.widened) {
    return;
  }
  // A plan narrower than the guest's own view would squash the picture; compare against this publication's view.
  if (plan_.projectionExtent.width <= retail.nativeWidth) {
    lucent::warn("ctr-projection",
                 "plan REFUSED: its projection extent {} is not wider than the guest's own view width "
                 "{}. Publishing it would narrow the picture, not widen it, so retail's triple "
                 "({}) stands.",
                 plan_.projectionExtent.width,
                 retail.nativeWidth,
                 native::kStartupLiteralPublication);
    published_ = GteProjection{
        .centerX = retail.centerX,
        .centerY = retail.centerY,
        .distance = static_cast<int32_t>(retail.screenDistance),
    };
    return;
  }
  // Host state only; the guest's view descriptor keeps its retail values.
  libgte_set_geom_offset(&core, widened.published.centerX, widened.published.centerY);
  libgte_set_geom_screen(&core, widened.published.distance);
  published_ = widened.published;
  widened_ = true;
}

} // namespace ctr
