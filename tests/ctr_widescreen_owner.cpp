// ctr_widescreen_owner.cpp — CTR's widening decision, and the census that made it necessary.
//
// TWO THINGS ARE PINNED HERE, and the second is the one that did not exist before.
//
// (1) `CtrWidescreen::presentationAspect` is the title's answer to the framework's question, and it
//     is the MEASURED root cause of CTR not widening: while `CtrRuntime::guestWidescreenProjection()`
//     returned the base nullptr, `gpu_vk_latch_guest_projection` always resolved
//     `requested = Standard4x3` and every plan was 4:3 whatever the settings file said. These cases
//     are the four settings values, and the fifth is the invalid one that must refuse rather than
//     silently resolve to 4:3 — a knob whose value matched nothing has to be reported as matching
//     nothing, never quietly defaulted.
//
// (2) The projection owner's PUBLICATION CENSUS. The owner speaks only on a disagreement, so before
//     the census a run in which it never fired printed nothing at all, and "no line" could not be
//     told from "never reached" — which is the question S004 and S006 both rest on. The census makes
//     the absent case a number, per source, against a field denominator.
//
// WHAT IS NOT COVERED, and it is a real gap rather than an oversight: no case here observes a real
// run's fault or its GPU, so the census is pinned against synthetic publications. The live numbers
// (176 publications over 29,034 fields, per source) are in docs/issues/0026 and came from the
// product, not from this file.

#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "hw_bind.h"
#include "projection_owner.h"
#include "widescreen_owner.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

int gChecks = 0;
int gFailures = 0;

void check(bool condition, const char *what) {
  ++gChecks;
  if (!condition) {
    ++gFailures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

template <typename A, typename E> void checkEq(A actual, E expected, const char *what) {
  ++gChecks;
  if (static_cast<long long>(actual) != static_cast<long long>(expected)) {
    ++gFailures;
    std::fprintf(stderr,
                 "  FAIL: %s (got %lld, expected %lld)\n",
                 what,
                 static_cast<long long>(actual),
                 static_cast<long long>(expected));
  }
}

constexpr uint32_t kScreenDistanceOffset = 0x18u;
constexpr uint32_t kWidthOffset = 0x20u;
constexpr uint32_t kHeightOffset = 0x22u;

void writeView(Core &core, uint32_t view, uint16_t width, uint16_t height, uint32_t screenDistance) {
  core.mem_w32(view + kScreenDistanceOffset, screenDistance);
  core.mem_w16(view + kWidthOffset, width);
  core.mem_w16(view + kHeightOffset, height);
}

void publishAgreeing(Core &core) {
  const uint32_t view = core.r[4];
  core.rsub.projParams.setGeomOffset(static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kWidthOffset))) / 2.0f,
                                     static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kHeightOffset))) /
                                         2.0f);
  core.rsub.projParams.setGeomScreen(static_cast<float>(core.mem_r32(view + kScreenDistanceOffset)));
}

// ---- (1) the aspect answer --------------------------------------------------------------------------

void test_aspect_answer_follows_the_settings_value() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  ctr::CtrWidescreen wide;

  struct Case {
    int setting;
    PresentationAspect expected;
    const char *name;
  };
  const Case cases[] = {
      {ASPECT_4_3, PresentationAspect::Standard4x3, "aspect=0 is 4:3"},
      {ASPECT_16_9, PresentationAspect::Wide16x9, "aspect=1 is 16:9"},
      {ASPECT_21_9, PresentationAspect::UltraWide21x9, "aspect=2 is 21:9"},
      {ASPECT_AUTO, PresentationAspect::MatchSink, "aspect=3 matches the sink"},
  };
  for (const Case &entry : cases) {
    game->mods.aspect = entry.setting;
    check(wide.presentationAspect(core) == entry.expected, entry.name);
  }
  // The runtime must HAND BACK this answer. Returning the base nullptr made every plan 4:3 whatever
  // the settings file said, which is the defect this whole owner exists to close, so the binding is
  // asserted rather than assumed. It is the RUNTIME's own instance that is handed back, so that is
  // the one the rest of the product resolves against.
  check(runtime.guestWidescreenProjection() == &runtime.widescreen(),
        "the runtime hands back CTR's own guest projection, not the base nullptr");
  check(runtime.guestWidescreenProjection() != nullptr,
        "a null guest projection is the defect; null must never be what the runtime returns");
  check(!runtime.widescreen().latched(), "the plan is resolved on the first publication, not at construction");
}

// ---- (2) the publication census ---------------------------------------------------------------------

void test_publication_census_counts_per_source() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 512, 216, 0x2000u);

  ctr::ProjectionOwner owner;
  // THREE FIELDS, only two of which publish. A census whose denominator is the publication count
  // rather than the field count cannot express "two of three fields published", which is the
  // statement a reader actually needs.
  owner.beginField();
  owner.beginField();
  owner.beginField();
  checkEq(static_cast<long long>(owner.census().fields), 3, "every beginField counts a field");
  checkEq(static_cast<long long>(owner.census().publications), 0, "a fresh owner has published nothing");

  core.r[4] = kView;
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::Overlay);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::StateZero);

  const auto &census = owner.census();
  checkEq(static_cast<long long>(census.publications), 2, "both publications are counted");
  checkEq(static_cast<long long>(census.fromOverlay), 1, "the overlay caller is counted separately");
  checkEq(static_cast<long long>(census.fromStateZero), 1, "the state-zero caller is counted separately");
  checkEq(static_cast<long long>(census.fromLensFlare),
          0,
          "a source that never ran reads as an explicit zero, not as an absent row");
  check(census.bootGteTripleSampled, "the pre-retail GTE sample is taken on the first publication");
  checkEq(census.widenedPublications, 0, "an unlatched plan widens nothing");
  checkEq(static_cast<long long>(census.fields), 3, "publishing does not advance the field count");
}

void test_first_publication_samples_the_boot_triple() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 512, 216, 0x2000u);
  core.r[4] = kView;

  // Pretend the boot already published retail's literal triple (0x8003C84C: OFX=256, OFY=120,
  // H=320), which is what the live run measured at the first publication.
  core.rsub.projParams.setGeomOffset(256.0f, 120.0f);
  core.rsub.projParams.setGeomScreen(320.0f);

  ctr::ProjectionOwner owner;
  owner.beginField();
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::StateZero);
  const auto &census = owner.census();
  check(census.bootGteTripleSampled, "the boot triple was sampled before the retail body ran");
  check(census.bootGteTripleValid, "the sampled boot triple is a valid published one");
  // The sample happens BEFORE `retailBody`, so it is retail's own boot value and not the view's.
  checkEq(census.bootGteTriple.centerX, 256, "the sampled boot OFX is retail's 256, not the view's");
  checkEq(census.bootGteTriple.centerY, 120, "the sampled boot OFY is retail's 120");
  checkEq(census.bootGteTriple.distance, 320, "the sampled boot H is retail's 320");
  checkEq(owner.current().centerX, 256, "the published OFX is the view's own 512/2");
}

// ---- (3) the plan source is per-publication, and writes no guest byte -------------------------------

void test_plan_source_widens_from_the_guests_own_view() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  // 512x216 is CTR's MEASURED view (the boot triple's OFX=256 is 512/2). A 320-wide plan against
  // this view is NARROWER than the native picture, which is the defect the first version shipped.
  writeView(core, kView, 512, 216, 0x2000u);
  core.r[4] = kView;

  ctr::ProjectionOwner owner;
  ctr::CtrWidescreen wide;
  int asked = 0;
  owner.setPlanSource([&asked](Core &, const ctr::GuestViewProjection &view) {
    ++asked;
    checkEq(view.width, 512, "the plan source is handed the guest's own view width");
    checkEq(view.height, 216, "the plan source is handed the guest's own view height");
    GuestProjectionPlan plan{};
    plan.nativeExtent = {512, 216};
    plan.nativeProjectionExtent = {512, 216};
    plan.presentationExtent = {684, 216};
    plan.projectionExtent = {684, 216};
    plan.projectionCenterX = 342;
    plan.aspect = PresentationAspect::Wide16x9;
    return plan;
  });

  // The guest's own descriptor BEFORE the owner does anything to it.
  const uint16_t guestWidthBefore = core.mem_r16(kView + kWidthOffset);
  const uint16_t guestHeightBefore = core.mem_r16(kView + kHeightOffset);
  const uint32_t guestDistanceBefore = core.mem_r32(kView + kScreenDistanceOffset);

  owner.beginField();
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::Overlay);

  checkEq(asked, 1, "the plan source is asked exactly once, on the first publication");
  check(owner.widenedLastPublication(), "a plan wider than the view widens the publication");
  checkEq(owner.publishedProjection().centerX, 342, "the widened OFX is the plan's own centre");
  checkEq(owner.publishedProjection().distance, 0x2000 * 684 / 512, "H is scaled by the plan's own factor");
  checkEq(static_cast<long long>(owner.census().widenedPublications), 1, "the census records the widening");

  // THE INVARIANT THAT MAKES WIDENING LEGAL HERE. The plan reaches the GTE control registers and
  // nothing else: the guest's descriptor keeps retail's values, so no gameplay read of it can be
  // affected. A plan that wrote the descriptor turns these three red.
  checkEq(core.mem_r16(kView + kWidthOffset), guestWidthBefore, "the guest's view width is unchanged");
  checkEq(core.mem_r16(kView + kHeightOffset), guestHeightBefore, "the guest's view height is unchanged");
  checkEq(core.mem_r32(kView + kScreenDistanceOffset), guestDistanceBefore, "the guest's H word is unchanged");
}

void test_a_plan_narrower_than_the_view_does_not_widen() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 512, 216, 0x2000u);
  core.r[4] = kView;

  ctr::ProjectionOwner owner;
  owner.setPlanSource([](Core &, const ctr::GuestViewProjection &) {
    // The exact plan the first version of the owner built: 320 -> 428, against a 512-wide picture.
    GuestProjectionPlan plan{};
    plan.nativeExtent = {320, 240};
    plan.nativeProjectionExtent = {320, 240};
    plan.presentationExtent = {428, 240};
    plan.projectionExtent = {428, 240};
    plan.projectionCenterX = 214;
    return plan;
  });
  owner.beginField();
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::Overlay);
  check(!owner.widenedLastPublication(),
        "a projection extent narrower than the guest's own view is refused as a widening");
  checkEq(owner.publishedProjection().centerX, 256, "a refused plan leaves retail's OFX in place");
  checkEq(owner.publishedProjection().distance, 0x2000, "a refused plan leaves retail's H in place");
}

} // namespace

int main() {
  test_aspect_answer_follows_the_settings_value();
  test_publication_census_counts_per_source();
  test_first_publication_samples_the_boot_triple();
  test_plan_source_widens_from_the_guests_own_view();
  test_a_plan_narrower_than_the_view_does_not_widen();
  std::fprintf(stderr, "ctr_widescreen_owner: %d/%d checks passed\n", gChecks - gFailures, gChecks);
  return gFailures == 0 ? 0 : 1;
}
