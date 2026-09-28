// CTR's second projection application point: the ten geometry submitters' own publication of the
// GTE projection triple.
//
// WHY THIS EXISTS. The widescreen owner widens the GTE at ONE publication (0x80042910) and the canvas
// widens, and the picture does not. The reason, measured by `tools/ctr_binary_probe.py gte-projection`
// and quoted in docs/issues/0031, is that the guest writes the same triple itself at ten more sites:
// sixteen raw `ctc2 rX,$26` words and two `jal SetGeomScreen` calls write H in total, and ten of the
// raw writes are inside the geometry-submission functions. `CtrGeometryProjectionOwner` applies the
// owned plan at the measured moment the GTE consumes the triple, so the widening reaches 3D geometry.
//
// WHAT IS COVERED, and each item is one way this owner could be wrong in a way that would be invisible:
//
//   * the GTE control-register NUMBERS, pinned against the framework's own writer with a positive AND
//     a negative round trip — `libgte_set_geom_*` is the only writer, so if the numbers here drifted
//     from the framework's, the widening would land in a register the transform never reads and this
//     suite would still pass on its own arithmetic;
//   * IDEMPOTENCE, which is the failure mode that would corrupt a frame without any assertion firing:
//     a submission function publishes ONCE and then runs many perspective transforms, so a
//     non-idempotent widening compounds as 1.34^n inside a single draw call;
//   * the census, including the "armed and saw nothing" case, because a silent owner cannot tell
//     "no 3D geometry" from "the seam never fired" and the second is the reading that would be
//     believed;
//   * a 4:3 plan being inert, so the shipping default changes nothing at all.
//
// NO GAME, NO DISC, NO WINDOW: one `Game`, the GTE control registers written directly, and the
// projection owner's plan latched from a test-supplied view.
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "geometry_projection_owner.h"
#include "hw_bind.h"
#include "projection_owner.h"

#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
  if (!condition) {
    ++failures;
  }
  std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
}

// The two GTE ops the owner admits, and one it must not. RTPS is the single-vertex form this image
// uses; RTPT is the three-vertex form; 0x0C is the vector/outer-product op, which consumes no H.
constexpr uint32_t kRtps = 0x01u << 5;     // sub-op 0x01, bits 5..10 of the word
constexpr uint32_t kRtpt = 0x30u << 5;     // sub-op 0x30
constexpr uint32_t kVectorOp = 0x0Cu << 5; // sub-op 0x0C

// `widenDistance` in view_projection_plan.cpp rounds to nearest, so the test must ask the same
// question rather than a truncating one. Duplicating the ARITHMETIC would be a second
// implementation of a rule that has exactly one; this asks the owner's own readback instead.
constexpr int32_t kRetailH = 320;
constexpr int32_t kWidenedH = 428; // widenDistance(320, 684, 512)

} // namespace

int main() {
  using ctr::CtrGeometryProjectionOwner;
  using ctr::ProjectionOwner;
  using ctr::ScopedGteProjectionObservation;
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The owner reads the GTE control registers, and `libgte_set_geom_*` writes the BOUND ones, so the
  // test binds the same way game/app/main.cpp does rather than avoiding the writes.
  gte_bind(&core);

  std::printf("[ctr-geometry-projection] THE REGISTER NUMBERS, against the framework's own writer\n");
  // The framework's `libgte_set_geom_*` is the only writer of these three, and `CtrGeometryProjectionOwner`
  // names them independently. If the two ever disagreed, the owner would widen a register the transform
  // does not read and every arithmetic assertion below would still pass. So pin the writer, not the
  // owner's intent: publish, read back, and require both halves to agree.
  libgte_set_geom_offset(&core, 300, 120);
  libgte_set_geom_screen(&core, 455);
  {
    const auto live = CtrGeometryProjectionOwner::readLiveTriple();
    // The centres are the RAW 16.16 words, not screen offsets: the guest's `SetGeomOffset` and its ten
    // geometry submitters scale the same displacement by `<<16` and `<<15` respectively, so there is
    // no scale this reader could pick that would be right for both. Pinning the raw word is what makes
    // that an explicit decision rather than a hidden assumption.
    check(live.centerX == 300 << 16 && live.centerY == 120 << 16,
          "the centres read back as the raw 16.16 control-register words the framework wrote");
    check(live.distance == 455, "H round trips through libgte_set_geom_screen unshifted");
  }
  // NEGATIVE, so a readLiveTriple that always echoed its own last argument cannot pass. A different H
  // must be a different reading.
  libgte_set_geom_screen(&core, 455);
  {
    const auto live = CtrGeometryProjectionOwner::readLiveTriple();
    check(live.distance == 455, "re-publishing the same H reads the same H (no state carried in the read)");
  }
  libgte_set_geom_screen(&core, 320);
  {
    const auto live = CtrGeometryProjectionOwner::readLiveTriple();
    check(live.distance == 320, "a DIFFERENT H reads back as the different H");
  }

  std::printf("[ctr-geometry-projection] THE CENSUS, including the case that must not read as a pass\n");
  ProjectionOwner projection;
  CtrGeometryProjectionOwner owner;
  owner.setProjectionOwner(&projection);
  {
    // Armed and offered no GTE op at all. The owner's report must be able to say so; a run that
    // printed "0 projections widened" here would be indistinguishable from "no 3D geometry exists".
    const ScopedGteProjectionObservation observation(core, owner);
  }
  check(owner.census().fields == 1, "one field was counted while armed");
  check(owner.census().gteOpsSeen == 0, "no GTE op was offered, so the denominator is 0");
  check(owner.census().projections == 0, "and no perspective transform was counted");

  std::printf("[ctr-geometry-projection] A 4:3 PLAN IS INERT: the shipping default changes nothing\n");
  {
    ProjectionOwner inert;
    // A 4:3 plan whose projection extent equals its native extent, latched exactly the way the frame
    // driver latches it.
    ::GuestProjectionPlan plan;
    plan.nativeProjectionExtent.width = 512;
    plan.nativeProjectionExtent.height = 240;
    plan.projectionExtent.width = 512;
    plan.projectionExtent.height = 240;
    plan.projectionCenterX = 256;
    inert.latchPlan(plan, 1, 1);
    CtrGeometryProjectionOwner fourThree;
    fourThree.setProjectionOwner(&inert);
    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    const auto outcome = fourThree.applyOwnedPlan(core, CtrGeometryProjectionOwner::readLiveTriple());
    const auto live = CtrGeometryProjectionOwner::readLiveTriple();
    check(outcome == CtrGeometryProjectionOwner::Outcome::LeftAtRetail,
          "a 4:3 plan is LeftAtRetail, which is a different fact from AlreadyOwned");
    check(live.distance == 320 && live.centerX == (256 << 16) && live.centerY == (120 << 16),
          "retail's triple is left exactly as the guest published it, centre included");
  }

  std::printf("[ctr-geometry-projection] A 16:9 PLAN WIDENS, ONCE, AND NOT AGAIN\n");
  {
    ProjectionOwner wide;
    ::GuestProjectionPlan plan;
    plan.nativeProjectionExtent.width = 512;
    plan.nativeProjectionExtent.height = 240;
    plan.projectionExtent.width = 684;
    plan.projectionExtent.height = 240;
    plan.projectionCenterX = 342;
    // The measured scale the widescreen owner applies: 684/512 reduced, as a fraction.
    wide.latchPlan(plan, 684, 512);
    CtrGeometryProjectionOwner ownerWide;
    ownerWide.setProjectionOwner(&wide);

    // The guest publishes retail, then runs a run of perspective transforms through it.
    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kVectorOp);
      check(ownerWide.census().gteOpsSeen == 1 && ownerWide.census().projections == 0,
            "a non-perspective GTE op counts in the denominator and not as a projection");
      ownerWide.observe(core, kRtps);
    }
    const auto afterFirst = CtrGeometryProjectionOwner::readLiveTriple();
    check(ownerWide.census().projections == 1, "one perspective transform consumed the triple");
    check(ownerWide.census().lastRetailDistance == kRetailH,
          "retail's H is recorded at the widening, before this owner's write overwrote it");
    check(afterFirst.distance == kWidenedH, "the first perspective transform ran with the WIDENED H, not retail's");
    // The centre is deliberately NOT re-written here. `widenViewProjection` re-centres, but the two
    // guest publication sites scale the centre differently (<<16 vs <<15), so applying it here would
    // be a guess. The shift this leaves in the 3D path is measured in docs/issues/0031.
    check(afterFirst.centerX == 256 << 16,
          "the centre is left exactly as the guest published it, not re-centred by a guessed scale");

    // The same submission function's SECOND vertex: the registers still hold what the owner published
    // and the guest has not republished. Compounding here is the failure that no arithmetic assertion
    // in this file would catch on its own.
    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kRtpt);
    }
    const auto afterSecond = CtrGeometryProjectionOwner::readLiveTriple();
    check(afterSecond.distance == afterFirst.distance,
          "the second perspective transform did NOT compound the widening");
    check(ownerWide.census().projections == 2 && ownerWide.census().widenedProjections == 1 &&
              ownerWide.census().alreadyOwned == 1,
          "two projections seen, one widened and one AlreadyOwned");

    // And when the guest DOES republish, the owner widens again from the new retail value.
    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kRtps);
    }
    const auto afterRepublish = CtrGeometryProjectionOwner::readLiveTriple();
    check(ownerWide.census().projections == 3 && ownerWide.census().widenedProjections == 2,
          "a guest republication is widened again, from the retail value");
    check(afterRepublish.distance == afterFirst.distance,
          "and it lands on the same widened H rather than on the previous frame's");
  }

  std::printf("[ctr-geometry-projection] THE CENSUS REPORTS ITS OWN DENOMINATOR\n");
  {
    ProjectionOwner wide;
    ::GuestProjectionPlan plan;
    plan.nativeProjectionExtent.width = 512;
    plan.nativeProjectionExtent.height = 240;
    plan.projectionExtent.width = 684;
    plan.projectionExtent.height = 240;
    plan.projectionCenterX = 342;
    wide.latchPlan(plan, 684, 512);
    CtrGeometryProjectionOwner counted;
    counted.setProjectionOwner(&wide);
    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    {
      const ScopedGteProjectionObservation observation(core, counted);
      counted.observe(core, kRtps);
      counted.observe(core, kVectorOp);
    }
    const auto census = counted.census();
    check(census.fields == 1, "1 field");
    check(census.gteOpsSeen == 2, "2 GTE ops offered, both counted");
    check(census.projections == 1, "1 of them was a perspective transform");
    check(census.widenedProjections == 1, "and it was widened");
    check(census.lastRetailDistance == kRetailH, "the last RETAIL H is reported, not the widened one");
    check(census.widenedProjections + census.alreadyOwned + census.leftAtRetail + census.projectionsBeforePlan +
                  census.refusedNoPlanOwner ==
              census.projections,
          "the five outcomes are EXHAUSTIVE and sum to the projection count");
    check(census.lastPublishedDistance == kWidenedH, "the last PUBLISHED H is reported too");
    counted.reportCensus();
  }

  std::printf("\n");
  if (failures != 0) {
    std::printf("[ctr-geometry-projection] %d FAILURE(S)\n", failures);
    return 1;
  }
  std::printf("[ctr-geometry-projection] all checks passed\n");
  return 0;
}
