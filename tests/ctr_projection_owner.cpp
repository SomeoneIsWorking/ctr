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
//   * NOT CLAIMED — that the offsets are CORRECT. This test pins the owner's behaviour against the
//     numbers in its own source. Verifying them means reading the retail image, and CTR has no disc
//     provisioned on this machine, so nothing here is evidence about Crash Team Racing's binary.
//
// NO GAME, NO DISC, NO WINDOW: one `Game`, guest memory written directly, and a retail body supplied by
// the test that publishes whatever the test wants.
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
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
  core.rsub.projParams.setGeomOffset(
      static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kWidthOffset))) / 2.0f,
      static_cast<float>(static_cast<int16_t>(core.mem_r16(view + kHeightOffset))) / 2.0f);
  core.rsub.projParams.setGeomScreen(static_cast<float>(core.mem_r32(view + kScreenDistanceOffset)));
}

// (1) The publication carries what the owner read, and derives the centre by halving.
void test_publication_carries_the_views_own_numbers() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 320, 240, 0x1000u);
  core.r[4] = kView;

  ctr::ProjectionOwner owner;
  check(!owner.current().valid(), "a default publication is not valid");
  check(!owner.previous().valid(), "previous() is empty before the first publication");
  owner.publish(core, publishAgreeing);

  const auto &now = owner.current();
  checkEq(now.source, kView, "source is the view pointer the guest passed in $a0");
  checkEq(now.nativeWidth, 320, "nativeWidth is the view's width halfword");
  checkEq(now.nativeHeight, 240, "nativeHeight is the view's height halfword");
  checkEq(now.centerX, 160, "centerX is width/2 — this is the owner's OFX");
  checkEq(now.centerY, 120, "centerY is height/2 — the owner's OFY");
  checkEq(now.screenDistance, 0x1000, "screenDistance is the view's word at +0x18 — the owner's H");
  checkEq(static_cast<long long>(now.sequence), 1, "the first publication is sequence 1");
  check(now.valid(), "a publication read from a real view is valid");
  std::fprintf(stderr, "  view 0x%08X -> %dx%d centre(%d,%d) H=%u seq=%llu\n", kView, now.nativeWidth,
               now.nativeHeight, now.centerX, now.centerY, now.screenDistance,
               static_cast<unsigned long long>(now.sequence));
}

// (2) `previous()` lags by exactly one publication, and the sequence advances — the property a consumer
//     reads to notice that the projection CHANGED rather than to read the current value twice.
void test_previous_lags_and_the_sequence_advances() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  constexpr uint32_t kView = 0x800A0000u;
  ctr::ProjectionOwner owner;

  writeView(core, kView, 320, 240, 0x1000u);
  core.r[4] = kView;
  owner.publish(core, publishAgreeing);
  check(!owner.previous().valid(), "previous() is still empty after exactly one publication");

  // A second publication with DIFFERENT numbers: this is the transition a widescreen owner would make.
  writeView(core, kView, 368, 240, 0x1200u);
  owner.publish(core, publishAgreeing);
  checkEq(owner.previous().nativeWidth, 320, "previous() holds the FIRST publication's width");
  checkEq(owner.previous().screenDistance, 0x1000, "previous() holds the FIRST publication's H");
  checkEq(owner.current().nativeWidth, 368, "current() holds the second publication's width");
  checkEq(owner.current().centerX, 184, "current() re-derives the centre from the new width");
  checkEq(static_cast<long long>(owner.current().sequence), 2, "the sequence advanced to 2");
  std::fprintf(stderr, "  transition 320x240/H=0x1000 -> %dx%d/H=0x%X, previous() kept the old one\n",
               owner.current().nativeWidth, owner.current().nativeHeight,
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
  constexpr uint32_t kView = 0x800A0000u;
  writeView(core, kView, 256, 224, 0x0F00u);
  core.r[4] = kView;
  ctr::ProjectionOwner owner;
  ctr::ProjectionOwner::RetailBody body = publishAgreeing;
  check(static_cast<bool>(body), "a callable body is what publish requires");
  owner.publish(core, body);
  checkEq(owner.current().centerX, 128, "a 256-wide view centres at 128");
  checkEq(owner.current().centerY, 112, "a 224-high view centres at 112");
}

} // namespace

int main() {
  std::fprintf(stderr, "ctr projection owner\n");
  test_publication_carries_the_views_own_numbers();
  test_previous_lags_and_the_sequence_advances();
  test_publication_requires_a_body_and_accepts_one();
  if (failures) {
    std::fprintf(stderr, "%d check(s) FAILED\n", failures);
    return 1;
  }
  std::fprintf(stderr, "PASS: the owner reads 0x18/0x20/0x22, halves for the centre, and lags previous() "
                       "by one publication. NOT covered: the std::abort() refusal, which has no "
                       "death-test facility in this suite.\n");
  return 0;
}
