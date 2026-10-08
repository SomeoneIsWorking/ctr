// CtrGeometryProjectionOwner: widens H at perspective GTE ops, never compounding.

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

constexpr uint32_t kRtps = 0x01u << 5;     // sub-op 0x01, bits 5..10 of the word
constexpr uint32_t kRtpt = 0x30u << 5;     // sub-op 0x30
constexpr uint32_t kVectorOp = 0x0Cu << 5; // sub-op 0x0C

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
  gte_bind(&core);

  std::printf("[ctr-geometry-projection] THE REGISTER NUMBERS, against the framework's own writer\n");
  libgte_set_geom_offset(&core, 300, 120);
  libgte_set_geom_screen(&core, 455);
  {
    const auto live = CtrGeometryProjectionOwner::readLiveTriple();
    check(live.centerX == 300 << 16 && live.centerY == 120 << 16,
          "the centres read back as the raw 16.16 control-register words the framework wrote");
    check(live.distance == 455, "H round trips through libgte_set_geom_screen unshifted");
  }
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

  ProjectionOwner projection;
  CtrGeometryProjectionOwner owner;
  owner.setProjectionOwner(&projection);

  std::printf("[ctr-geometry-projection] A 4:3 PLAN IS INERT: the shipping default changes nothing\n");
  {
    ProjectionOwner inert;
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
    wide.latchPlan(plan, 684, 512);
    CtrGeometryProjectionOwner ownerWide;
    ownerWide.setProjectionOwner(&wide);

    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kVectorOp);
      ownerWide.observe(core, kRtps);
    }
    const auto afterFirst = CtrGeometryProjectionOwner::readLiveTriple();
    check(afterFirst.distance == kWidenedH, "the first perspective transform ran with the WIDENED H, not retail's");
    check(afterFirst.centerX == 256 << 16,
          "the centre is left exactly as the guest published it, not re-centred by a guessed scale");

    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kRtpt);
    }
    const auto afterSecond = CtrGeometryProjectionOwner::readLiveTriple();
    check(afterSecond.distance == afterFirst.distance,
          "the second perspective transform did NOT compound the widening");

    libgte_set_geom_offset(&core, 256, 120);
    libgte_set_geom_screen(&core, 320);
    {
      const ScopedGteProjectionObservation observation(core, ownerWide);
      ownerWide.observe(core, kRtps);
    }
    const auto afterRepublish = CtrGeometryProjectionOwner::readLiveTriple();
    check(afterRepublish.distance == afterFirst.distance,
          "and it lands on the same widened H rather than on the previous frame's");
  }

  std::printf("\n");
  if (failures != 0) {
    std::printf("[ctr-geometry-projection] %d FAILURE(S)\n", failures);
    return 1;
  }
  std::printf("[ctr-geometry-projection] all checks passed\n");
  return 0;
}
