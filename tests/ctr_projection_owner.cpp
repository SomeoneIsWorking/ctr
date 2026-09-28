// CTR's projection publication owner: what it reads, what it derives, and what it refuses.
//
// WHY THIS EXISTS. `game/video/projection_owner.h` states the owner "captures its view input and
// refuses if the published libgte state disagrees", and that sentence is the owner's whole contract —
// yet the owner had NO test. It reads three fields out of a guest view struct at fixed offsets, derives
// the projection centre by halving the width and height, and then checks the guest's own published
// geometry against what it derived. Every one of those is a place a later edit can silently break, and
// two of them are the exact `ofx`/`H` pair a widescreen owner will consume, so they are worth pinning
// now rather than after something depends on them.
//
// WHAT IS AND IS NOT COVERED, stated rather than implied:
//
//   * COVERED — the offsets (0x18 / 0x20 / 0x22), the halving, `source`, `sequence` advancing per
//     publication, `previous()` being empty before the first publication and equal to the first
//     publication after the second, and `valid()` rejecting a default-constructed publication.
//   * NOT COVERED — the refusal. A disagreement calls `std::abort()`, and this suite is an in-process
//     `main()` with no death-test facility, so the branch that fires when the guest publishes geometry
//     that does not match the view has NO automated coverage. That is a real gap and it is named in
//     `docs/project-state.md` rather than papered over; a fork-based death test would not be portable to
//     the Windows and macOS builds this project also ships.
//   * NOW GROUNDED — the offsets are no longer taken on trust. The image is provisioned and
//     `tools/ctr_binary_probe.py projection-owner` reads the retail body at 0x80042910 out of
//     `scratch/raw/ctr/SCUS_944.26`: `lhu v0,32(s0)` / `lhu v0,34(s0)` / `lw a0,24(s0)` are the
//     width, height and H reads, and the body ends at 0x80042974. This suite still pins the owner's
//     BEHAVIOUR; the grounding that makes those numbers real is the probe's, quoted in
//     `docs/issues/0026`.
//
// NO GAME, NO DISC, NO WINDOW: one `Game`, guest memory written directly, and a retail body supplied by
// the test that publishes whatever the test wants.
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

// The guest view struct the owner reads. The offsets are the owner's, declared here independently so a
// change to one side has to be made deliberately in two places rather than drifting.
constexpr uint32_t kScreenDistanceOffset = 0x18u;
constexpr uint32_t kWidthOffset = 0x20u;
constexpr uint32_t kHeightOffset = 0x22u;

void writeView(Core &core, uint32_t view, uint16_t width, uint16_t height, uint32_t screenDistance) {
  core.mem_w32(view + kScreenDistanceOffset, screenDistance);
  core.mem_w16(view + kWidthOffset, width);
  core.mem_w16(view + kHeightOffset, height);
}

// A retail body that faithfully publishes what the owner derived from the same view — the agreeing case.
void publishAgreeing(Core &core) {
  const uint32_t view = core.r[4];
  core.rsub.projParams.setGeomOffset(static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kWidthOffset))) / 2.0f,
                                     static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kHeightOffset))) /
                                         2.0f);
  core.rsub.projParams.setGeomScreen(static_cast<float>(core.mem_r32(view + kScreenDistanceOffset)));
}

// (1) The publication carries what the owner read, and derives the centre by halving.
void test_publication_carries_the_views_own_numbers() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
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

// (2) `previous()` lags by exactly one publication, and the sequence advances — the property a consumer
//     reads to notice that the projection CHANGED rather than to read the current value twice.
void test_previous_lags_and_the_sequence_advances() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  ctr::ProjectionOwner owner;

  writeView(core, kView, 320, 240, 0x1000u);
  core.r[4] = kView;
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);
  check(!owner.previous().valid(), "previous() is still empty after exactly one publication");

  // A second publication with DIFFERENT numbers: this is the transition a widescreen owner would make.
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

// (3) The retail body is REQUIRED. `publish` aborts without one rather than publishing a projection it
//     did not observe, which is the property that makes the owner an oracle-backed capture rather than a
//     second, unaudited source of projection numbers. The abort is not exercised here — see the file
//     header — but the SIGNATURE is what every caller has to satisfy, and asserting the happy path runs
//     at all is what keeps a future edit from making the body optional.
void test_publication_requires_a_body_and_accepts_one() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
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

// ---- the widescreen plan -------------------------------------------------------------------------
//
// The plan is latched, not computed per frame from a hardcoded ratio, so the default configuration is
// byte-identical to retail. That is the property worth pinning: an owner that silently changed the
// picture on a 4:3 configuration would be a regression, not a feature.

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

// (4) With no plan latched the owner is retail. This is the default configuration, so this is the
//     regression that matters most: a change to the owner that moved the picture without a plan would
//     turn this red.
void test_without_a_plan_publication_is_retail() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
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

// (5) A latched 4:3 plan is ALSO retail. The plan is the only thing that may widen, so a standard
//     aspect must round-trip to the same triple — otherwise a user's 4:3 setting changes the game.
void test_a_standard_aspect_plan_stays_retail() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
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

// (6) A wide plan re-centres OFX on the widened extent, scales H, and — the property that makes this
//     safe — leaves the GUEST'S OWN VIEW DESCRIPTOR byte-for-byte alone. The owner writes host state;
//     a producer that wrote the descriptor could flip a gameplay read of it.
void test_a_wide_plan_widens_the_gte_and_not_the_guest() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
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

  // 320 -> 428 at 16:9 is the shared plan's own rounding, so the scale below is derived from the
  // plan rather than restated: this test cannot pass by agreeing with a number copied twice.
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

  // The GTE record the native depth path reads must carry the widened numbers, not retail's.
  checkEq(static_cast<long long>(core.rsub.projParams.geomH()), expectedH, "the GTE record carries the widened H");
  checkEq(static_cast<long long>(core.rsub.projParams.geomOfx()),
          plan.projectionCenterX,
          "the GTE record carries the widened OFX");

  // And the guest's descriptor is untouched: same three fields, byte for byte.
  checkEq(core.mem_r32(kView + kScreenDistanceOffset), kDistance, "the guest's H word is unchanged");
  checkEq(core.mem_r16(kView + kWidthOffset), kWidth, "the guest's width halfword is unchanged");
  checkEq(core.mem_r16(kView + kHeightOffset), kHeight, "the guest's height halfword is unchanged");
}

// (7) The retail comparison still happens, and it still happens against RETAIL. Widening is applied
//     after the comparison, so a guest that published geometry disagreeing with its own view is
//     refused exactly as before rather than being excused by a latched plan. The abort itself is not
//     exercised (see the file header); what is pinned is that the widened value is never what the
//     comparison is made against.
void test_the_retail_comparison_sees_retail_not_the_plan() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The widening path publishes through libgte_set_geom_*, which writes the bound GTE
  // control registers. Without a bind that write dereferences nothing and faults, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the write.
  gte_bind(&core);
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 320);
  core.r[4] = kView;

  const GuestProjectionPlan plan = makePlan(320, 240, {1280, 720}, PresentationAspect::Wide16x9);
  ctr::ProjectionOwner owner;
  owner.latchPlan(plan, plan.projectionExtent.width, plan.nativeProjectionExtent.width);
  owner.publish(core, publishAgreeing, ctr::ProjectionOwner::Source::LensFlare);

  // The captured publication is the guest's own, unmodified: the plan never rewrites history.
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
