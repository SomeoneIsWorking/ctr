// ProjectionOwner: view capture, centre derivation, sequence and previous().

#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "hw_bind.h"
#include "projection_owner.h"

#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "  FAIL %s\n", what);
  }
}

void checkEq(long long got, long long want, const char *what) {
  if (got != want) {
    ++failures;
    std::fprintf(stderr, "  FAIL %s: got %lld, expected %lld\n", what, got, want);
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

void test_publication_carries_the_views_own_numbers() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 0x1000u);
  core.r[4] = kView;

  ctr::ProjectionOwner owner;
  check(!owner.current().valid(), "a default publication is not valid");
  check(!owner.previous().valid(), "previous() is empty before the first publication");
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);

  const auto &now = owner.current();
  checkEq(now.source, kView, "source is the view pointer the guest passed in $a0");
  checkEq(now.nativeWidth, 320, "nativeWidth is the view's width halfword");
  checkEq(now.nativeHeight, 240, "nativeHeight is the view's height halfword");
  checkEq(now.centerX, 160, "centerX is width/2 — this is the owner's OFX");
  checkEq(now.centerY, 120, "centerY is height/2 — the owner's OFY");
  checkEq(now.screenDistance, 0x1000, "screenDistance is the view's word at +0x18 — the owner's H");
  checkEq(static_cast<long long>(now.sequence), 1, "the first publication is sequence 1");
  check(now.valid(), "a publication read from a real view is valid");
  std::fprintf(stderr,
               "  view 0x%08X -> %dx%d centre(%d,%d) H=%u seq=%llu\n",
               kView,
               now.nativeWidth,
               now.nativeHeight,
               now.centerX,
               now.centerY,
               now.screenDistance,
               static_cast<unsigned long long>(now.sequence));
}

void test_previous_lags_and_the_sequence_advances() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  ctr::ProjectionOwner owner;

  writeView(core, kView, 320, 240, 0x1000u);
  core.r[4] = kView;
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);
  check(!owner.previous().valid(), "previous() is still empty after exactly one publication");

  writeView(core, kView, 368, 240, 0x1200u);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);
  checkEq(owner.previous().nativeWidth, 320, "previous() holds the FIRST publication's width");
  checkEq(owner.previous().screenDistance, 0x1000, "previous() holds the FIRST publication's H");
  checkEq(owner.current().nativeWidth, 368, "current() holds the second publication's width");
  checkEq(owner.current().centerX, 184, "current() re-derives the centre from the new width");
  checkEq(static_cast<long long>(owner.current().sequence), 2, "the sequence advanced to 2");
  std::fprintf(stderr,
               "  transition 320x240/H=0x1000 -> %dx%d/H=0x%X, previous() kept the old one\n",
               owner.current().nativeWidth,
               owner.current().nativeHeight,
               owner.current().screenDistance);
}

void test_publication_requires_a_body_and_accepts_one() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 256, 224, 0x0F00u);
  core.r[4] = kView;
  ctr::ProjectionOwner owner;
  ctr::ProjectionOwner::RetailBody body = publishAgreeing;
  check(static_cast<bool>(body), "a callable body is what publish requires");
  owner.publish(core, body, ctr::ProjectionOwner::Source::Overlay);
  checkEq(owner.current().centerX, 128, "a 256-wide view centres at 128");
  checkEq(owner.current().centerY, 112, "a 224-high view centres at 112");
}

GuestProjectionPlan
makePlan(int nativeWidth, int nativeHeight, GuestPresentationExtent sink, PresentationAspect aspect) {
  GuestProjectionInputs inputs{};
  inputs.path = RenderPath::Gte;
  inputs.requested = aspect;
  inputs.nativePresentation = {nativeWidth, nativeHeight};
  inputs.nativeProjection.extent = {nativeWidth, nativeHeight};
  inputs.nativeProjection.drawWidth = nativeWidth;
  inputs.sink = sink;
  return guest_projection_plan(inputs);
}

void test_without_a_plan_publication_is_retail() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 0x140u);
  core.r[4] = kView;

  ctr::ProjectionOwner owner;
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);
  check(!owner.widenedLastPublication(), "no plan latched means the publication was not widened");
  checkEq(owner.publishedProjection().centerX, 160, "unlatched OFX is retail's width/2");
  checkEq(owner.publishedProjection().centerY, 120, "unlatched OFY is retail's height/2");
  checkEq(owner.publishedProjection().distance, 0x140, "unlatched H is retail's own value");
  std::fprintf(stderr,
               "  unlatched -> (%d,%d) H=%d\n",
               owner.publishedProjection().centerX,
               owner.publishedProjection().centerY,
               owner.publishedProjection().distance);
}

void test_a_standard_aspect_plan_stays_retail() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 0x140u);
  core.r[4] = kView;

  const GuestProjectionPlan plan = makePlan(320, 240, {1280, 720}, PresentationAspect::Standard4x3);
  check(!plan.widescreen(), "a 4:3 plan is not a widescreen plan");

  ctr::ProjectionOwner owner;
  owner.latchPlan(plan, 1, 1);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);
  check(!owner.widenedLastPublication(), "a 4:3 plan does not widen");
  checkEq(owner.publishedProjection().centerX, 160, "4:3 plan leaves OFX at retail's value");
  checkEq(owner.publishedProjection().distance, 0x140, "4:3 plan leaves H at retail's value");
}

void test_a_wide_plan_widens_the_gte_and_not_the_guest() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  constexpr uint16_t kWidth = 320;
  constexpr uint16_t kHeight = 240;
  constexpr uint32_t kDistance = 320;
  writeView(core, kView, kWidth, kHeight, kDistance);
  core.r[4] = kView;

  const GuestProjectionPlan plan = makePlan(kWidth, kHeight, {1280, 720}, PresentationAspect::Wide16x9);
  check(plan.widescreen(), "a 16:9 plan widens the projection extent");
  std::fprintf(stderr,
               "  plan: projectionExtent %dx%d centreX=%d (native %dx%d)\n",
               plan.projectionExtent.width,
               plan.projectionExtent.height,
               plan.projectionCenterX,
               plan.nativeProjectionExtent.width,
               plan.nativeProjectionExtent.height);

  const int32_t numerator = plan.projectionExtent.width;
  const int32_t denominator = plan.nativeProjectionExtent.width;

  ctr::ProjectionOwner owner;
  owner.latchPlan(plan, numerator, denominator);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);

  check(owner.widenedLastPublication(), "a wide plan widens the publication");
  checkEq(
      owner.publishedProjection().centerX, plan.projectionCenterX, "the widened OFX is the plan's projection centre");
  checkEq(owner.publishedProjection().centerY, kHeight / 2, "the vertical centre stays at the native half-height");
  const int64_t expectedH = (static_cast<int64_t>(kDistance) * numerator + denominator / 2) / denominator;
  checkEq(owner.publishedProjection().distance, expectedH, "H is scaled by the plan's horizontal ratio");
  std::fprintf(stderr,
               "  widened -> (%d,%d) H=%d (retail H=%u, scale %d/%d)\n",
               owner.publishedProjection().centerX,
               owner.publishedProjection().centerY,
               owner.publishedProjection().distance,
               kDistance,
               numerator,
               denominator);

  checkEq(static_cast<long long>(core.rsub.projParams.geomH()), expectedH, "the GTE record carries the widened H");
  checkEq(static_cast<long long>(core.rsub.projParams.geomOfx()),
          plan.projectionCenterX,
          "the GTE record carries the widened OFX");

  checkEq(core.mem_r32(kView + kScreenDistanceOffset), kDistance, "the guest's H word is unchanged");
  checkEq(core.mem_r16(kView + kWidthOffset), kWidth, "the guest's width halfword is unchanged");
  checkEq(core.mem_r16(kView + kHeightOffset), kHeight, "the guest's height halfword is unchanged");
}

void test_the_retail_comparison_sees_retail_not_the_plan() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 320);
  core.r[4] = kView;

  const GuestProjectionPlan plan = makePlan(320, 240, {1280, 720}, PresentationAspect::Wide16x9);
  ctr::ProjectionOwner owner;
  owner.latchPlan(plan, plan.projectionExtent.width, plan.nativeProjectionExtent.width);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);

  checkEq(owner.current().centerX, 160, "the captured publication is still retail's centre");
  checkEq(owner.current().screenDistance, 320, "the captured publication is still retail's H");
  checkEq(owner.current().nativeWidth, 320, "the captured width is the guest's, not the plan's");
}

} // namespace

int main() {
  std::fprintf(stderr, "ctr projection owner\n");
  test_publication_carries_the_views_own_numbers();
  test_previous_lags_and_the_sequence_advances();
  test_publication_requires_a_body_and_accepts_one();
  test_without_a_plan_publication_is_retail();
  test_a_standard_aspect_plan_stays_retail();
  test_a_wide_plan_widens_the_gte_and_not_the_guest();
  test_the_retail_comparison_sees_retail_not_the_plan();
  if (failures) {
    std::fprintf(stderr, "%d check(s) FAILED\n", failures);
    return 1;
  }
  std::fprintf(stderr,
               "PASS: the owner reads 0x18/0x20/0x22, halves for the centre, lags previous() "
               "by one publication, and is retail unless a WIDENING plan is latched — a wide "
               "plan re-centres OFX and scales H in the GTE record while leaving the guest's "
               "own view descriptor untouched. NOT covered: the std::abort() refusals (no "
               "death-test facility in this suite).\n");
  return 0;
}
