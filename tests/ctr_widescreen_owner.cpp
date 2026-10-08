// CtrWidescreen aspect answer and plan-driven projection publication.

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
  check(runtime.guestWidescreenProjection() == &runtime.widescreen(),
        "the runtime hands back CTR's own guest projection, not the base nullptr");
  check(runtime.guestWidescreenProjection() != nullptr,
        "a null guest projection is the defect; null must never be what the runtime returns");
  check(!runtime.widescreen().latched(), "the plan is resolved on the first publication, not at construction");
}

void test_plan_source_widens_from_the_guests_own_view() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
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

  const uint16_t guestWidthBefore = core.mem_r16(kView + kWidthOffset);
  const uint16_t guestHeightBefore = core.mem_r16(kView + kHeightOffset);
  const uint32_t guestDistanceBefore = core.mem_r32(kView + kScreenDistanceOffset);

  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::Overlay);

  checkEq(asked, 1, "the plan source is asked exactly once, on the first publication");
  check(owner.widenedLastPublication(), "a plan wider than the view widens the publication");
  checkEq(owner.publishedProjection().centerX, 342, "the widened OFX is the plan's own centre");
  checkEq(owner.publishedProjection().distance, 0x2000 * 684 / 512, "H is scaled by the plan's own factor");

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
    GuestProjectionPlan plan{};
    plan.nativeExtent = {320, 240};
    plan.nativeProjectionExtent = {320, 240};
    plan.presentationExtent = {428, 240};
    plan.projectionExtent = {428, 240};
    plan.projectionCenterX = 214;
    return plan;
  });
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::Overlay);
  check(!owner.widenedLastPublication(),
        "a projection extent narrower than the guest's own view is refused as a widening");
  checkEq(owner.publishedProjection().centerX, 256, "a refused plan leaves retail's OFX in place");
  checkEq(owner.publishedProjection().distance, 0x2000, "a refused plan leaves retail's H in place");
}

} // namespace

int main() {
  test_aspect_answer_follows_the_settings_value();
  test_plan_source_widens_from_the_guests_own_view();
  test_a_plan_narrower_than_the_view_does_not_widen();
  std::fprintf(stderr, "ctr_widescreen_owner: %d/%d checks passed\n", gChecks - gFailures, gChecks);
  return gFailures == 0 ? 0 : 1;
}
