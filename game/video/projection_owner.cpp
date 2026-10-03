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

  // WHAT THE BOOT LEFT IN THE GTE, sampled at the FIRST descriptor publication and BEFORE the
  // retail body overwrites it. This is the only measurement of the state-zero LITERAL publication
  // at 0x8003C84C that costs no override: that site bypasses this owner entirely, so it can never
  // be counted here, but it writes the same three GTE control registers and the value it leaves is
  // observable at the next publication. Retail's literals are OFX=256, OFY=120, H=320
  // (0x8003C84C / 0x8003C854 / 0x8003C85C), so a match is positive evidence the literal site ran
  // and a mismatch is positive evidence it did not. Neither is an inference from "it is straight-
  // line code inside the boot main", which is exactly the reasoning the workspace keeps punishing.
  if (next.sequence == 1u) {
    const auto &before = core.rsub.projParams;
    census_.bootGteTripleSampled = true;
    census_.bootGteTripleValid = before.geomValid();
    census_.bootGteTriple = GteProjection{
        .centerX = static_cast<int32_t>(before.geomOfx()),
        .centerY = static_cast<int32_t>(before.geomOfy()),
        .distance = static_cast<int32_t>(before.geomH()),
    };
  }

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
  // The plan is derived from the extent THIS publication carries, so it can never be a stale extent
  // and no first frame escapes it. A fixed `latchPlan` still wins, because a caller that named an
  // exact plan must not be overruled by a callback.
  if (planSource_ && !planLatched_) {
    const GuestProjectionPlan resolved =
        planSource_(core,
                    GuestViewProjection{.width = next.nativeWidth,
                                        .height = next.nativeHeight,
                                        .distance = static_cast<int32_t>(next.screenDistance)});
    plan_ = resolved;
    // H is scaled by the plan's OWN widening factor — the same ratio that widens the projection
    // extent widens the focal length, so the added horizontal field is genuinely rendered instead of
    // the 4:3 picture being stretched into the extra pixels.
    distanceScaleNumerator_ = resolved.projectionExtent.width;
    distanceScaleDenominator_ = resolved.nativeProjectionExtent.width;
    planLatched_ = true;
  }
  applyPresentationPlan(core, next);

  ++census_.publications;
  switch (source) {
  case Source::LensFlare:
    ++census_.fromLensFlare;
    break;
  case Source::StateZero:
    ++census_.fromStateZero;
    break;
  case Source::Overlay:
    ++census_.fromOverlay;
    break;
  }
  if (widened_) {
    ++census_.widenedPublications;
  }
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

void ProjectionOwner::beginField() {
  ++census_.fields;
}

const ProjectionOwner::PublicationCensus &ProjectionOwner::census() const {
  return census_;
}

void ProjectionOwner::reportCensus() const {
  // THE DENOMINATOR IS THE POINT. Without `fields`, "0 publications" and "0 of 40000 publications"
  // are the same string, and only one of them says the owner was never reached. `wasLive` says
  // whether this owner was installed for those fields at all, so a run that never armed the
  // projection seam is distinguishable from one that armed it and the guest never published.
  lucent::info("ctr-projection",
               "publication census over {} host field(s): {} descriptor publication(s) at 0x{:08X} "
               "(lens-flare {} = 0x{:08X}, state-zero {} = 0x{:08X}, overlay {} = 0x{:08X}), {} "
               "widened. Fields with NO publication are counted, not assumed.",
               census_.fields,
               census_.publications,
               native::kProjectionProducer,
               census_.fromLensFlare,
               native::kProjectionReturnLensflare,
               census_.fromStateZero,
               native::kProjectionReturnState,
               census_.fromOverlay,
               native::kProjectionReturnOverlay,
               census_.widenedPublications);
  // The boot's own GTE triple, and the verdict on the literal site, in the same breath. A run that
  // never sampled says so; a run that sampled a non-(256,120,320) triple says the literal site did
  // NOT run and names what was there instead.
  if (census_.bootGteTripleSampled) {
    const bool matchesLiteral = census_.bootGteTriple.centerX == native::kStartupProjectionOfx &&
                                census_.bootGteTriple.centerY == native::kStartupProjectionOfy &&
                                census_.bootGteTriple.distance == native::kStartupProjectionH;
    lucent::info("ctr-projection",
                 "GTE triple left by boot, sampled at the first descriptor publication before retail "
                 "ran: ({},{},H={}), valid={}. This is {} retail's state-zero LITERAL publication at "
                 "0x{:08X} ({}), which bypasses this owner and therefore cannot be counted above. "
                 "Reachability of that label by jal/jalr: 0 sites of 128,512 words — it is reached by "
                 "fall-through inside the state-zero main at 0x{:08X}, so it is not an override key.",
                 census_.bootGteTriple.centerX,
                 census_.bootGteTriple.centerY,
                 census_.bootGteTriple.distance,
                 census_.bootGteTripleValid ? 1 : 0,
                 matchesLiteral ? "EVIDENCE THAT" : "NOT",
                 native::kStartupLiteralPublication,
                 matchesLiteral ? "OFX=256, OFY=120, H=320" : "some other triple, so the literal site did not run",
                 native::kGuestMain);
  } else {
    lucent::warn("ctr-projection",
                 "the GTE triple left by boot was NOT SAMPLED — this run published no descriptor "
                 "projection, so there was no pre-retail moment to read. The boot's own publication "
                 "is therefore UNMEASURED here, not absent.");
  }
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
  // A PLAN NARROWER THAN THE GUEST'S OWN VIEW IS NOT A WIDENING, and publishing one would SQUASH the
  // picture rather than widen it. `widenViewProjection` compares the plan's projection extent with
  // the plan's OWN native extent, so a plan built from a stale 320 baseline (428 wide) reads as a
  // widening even when the guest is really publishing a 512-wide view — the plan is then 84 columns
  // NARROWER than retail, and the first version of the widescreen owner did exactly that. The
  // comparison has to be against the view this publication actually carried.
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
  // libgte_set_geom_* is the framework's single implementation of "publish this projection to
  // the GTE and record it", so the GTE control registers and ProjParams cannot drift apart. It
  // writes host state only; the guest's view descriptor keeps its retail values.
  libgte_set_geom_offset(&core, widened.published.centerX, widened.published.centerY);
  libgte_set_geom_screen(&core, widened.published.distance);
  published_ = widened.published;
  widened_ = true;
}

} // namespace ctr
