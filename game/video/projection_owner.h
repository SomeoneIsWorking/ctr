#pragma once

#include "view_projection_plan.h"

#include <cstdint>
#include <functional>

class Core;

namespace ctr {

struct ProjectionPublication {
  uint32_t source = 0;
  int32_t nativeWidth = 0;
  int32_t nativeHeight = 0;
  int32_t centerX = 0;
  int32_t centerY = 0;
  uint32_t screenDistance = 0;
  uint64_t sequence = 0;

  [[nodiscard]] bool valid() const {
    return source != 0 && nativeWidth > 0 && nativeHeight > 0 && screenDistance > 0;
  }
};

// Owns CTR's measured pre-GTE projection publication. The retail function remains the behavior
// oracle; this owner captures its view input, refuses if the published libgte state disagrees,
// and only then applies the latched host presentation plan. The widening is applied AFTER the
// retail check, so retail stays the oracle the refusal compares against, and it reaches GTE
// control registers only — the guest's own view descriptor is never written, so no gameplay read
// of it can be affected by the plan.
class ProjectionOwner final {
public:
  using RetailBody = std::function<void(Core &)>;

  // WHICH measured caller reached the owner. Kept as a named value rather than the bare return
  // address so the census tallies read as statements about the three call sites rather than as
  // hex the reader has to look up.
  enum class Source : uint8_t {
    LensFlare = 0, // 0x80024CCC, inside the measured lens-flare producer [0x80024C4C,0x80025138)
    StateZero = 1, // 0x8003BD2C
    Overlay = 2,   // 0x8003F5C0
  };
  [[nodiscard]] static uint32_t sourceReturnAddress(Source source);

  void publish(Core &core, const RetailBody &retailBody, Source source);

  [[nodiscard]] const ProjectionPublication &previous() const;
  [[nodiscard]] const ProjectionPublication &current() const;

  // WHERE THE PLAN COMES FROM, and why it is a per-publication callback rather than a value latched
  // once at boot.
  //
  // The plan's native extent must be the extent the GUEST actually projects into, and that is not
  // knowable before the guest says so: CTR's display mode arrives as GP1(08) during boot, and the
  // first version of the widescreen owner latched on the first field where `s_disp_w` was non-zero
  // and read 320, while the first PRESENT reported 512 for the same field counter. A plan latched
  // from 320 is 428 wide against a 512-wide picture — NARROWER than the thing it was meant to widen —
  // and `present_display_width` then declines to call that a widening, for a reason that has nothing
  // to do with widening.
  //
  // The descriptor the publication ALREADY reads is the authoritative native extent: the measured
  // rule is OFX = width / 2, and the boot literal OFX=256 pins the descriptor at 512 wide, which
  // agrees with GP1(08)=0x08000002 (`mode & 3 == 2` -> 512 dots). So the plan is derived from the
  // guest's own view on every publication, and the ordering problem disappears: there is no window in
  // which the plan can be derived from a stale extent, and no first frame that escapes it.
  using PlanSource = std::function<GuestProjectionPlan(Core &, const GuestViewProjection &)>;
  void setPlanSource(PlanSource source);

  // Latch a FIXED plan, for callers and tests that already know the extent. Overrides any plan
  // source, because a test that wants one exact plan must not be at the mercy of a callback.
  void latchPlan(const GuestProjectionPlan &plan, int32_t distanceScaleNumerator, int32_t distanceScaleDenominator);

  // What the last publication actually put in the GTE: retail when no plan is latched or the
  // plan does not widen.
  [[nodiscard]] const GteProjection &publishedProjection() const;
  [[nodiscard]] bool widenedLastPublication() const;

  // THE PUBLICATION CENSUS, and the reason it exists. `publish()` speaks only on a DISAGREEMENT,
  // because that is the only event a projection owner must fail loud on. A run in which the owner
  // never fired therefore prints NOTHING, and "no ctr-projection line" is indistinguishable from
  // "the owner was never reached" — which is the exact question S004 and S006 both depend on. These
  // counters make the absent case a number, and `beginField`/`endField` give it a DENOMINATOR, so a
  // run with zero publications reports zero OF N FIELDS rather than an unquantified silence.
  //
  // Per-source counts are kept apart because the three callers are not interchangeable: the
  // lens-flare path is the only one that runs inside a rendered frame, so "0 from the lens-flare
  // path" is a statement about the frame loop while "0 from state zero" is a statement about boot.
  struct PublicationCensus {
    uint64_t fields = 0;       // host fields the owner was live for
    uint64_t publications = 0; // descriptor-driven publications at 0x80042910
    uint64_t fromLensFlare = 0;
    uint64_t fromStateZero = 0;
    uint64_t fromOverlay = 0;
    uint64_t literalStartPublications = 0; // the 0x8003C84C leaf-pair publication
    uint64_t widenedPublications = 0;
    // The GTE triple the BOOT left behind, sampled at the first descriptor publication before the
    // retail body ran. `bootGteTripleSampled` says whether the sample was taken at all, so an
    // unsampled run cannot read as "the boot published nothing".
    GteProjection bootGteTriple{};
    bool bootGteTripleSampled = false;
    bool bootGteTripleValid = false;
  };

  // One host field begins. Counts the denominator the per-source tallies are a fraction of.
  void beginField();
  // One host field ends, and the census is reported. `reportCensus` is separate so a test can read
  // the counters without depending on the logger.
  void endField();
  [[nodiscard]] const PublicationCensus &census() const;
  void reportCensus() const;

  // Record the state-zero literal publication (0x8003C84C -> SetGeomOffset/SetGeomScreen with
  // OFX=256, OFY=120, H=320). It bypasses the descriptor entirely, so it can only be observed here.
  void noteLiteralStartupPublication(Core &core);

private:
  // Republish the GTE triple for the latched plan. Called only after the retail comparison has
  // passed, so a disagreement still refuses against retail rather than against the plan.
  void applyPresentationPlan(Core &core, const ProjectionPublication &retail);

  ProjectionPublication previous_{};
  ProjectionPublication current_{};
  GuestProjectionPlan plan_{};
  int32_t distanceScaleNumerator_ = 1;
  int32_t distanceScaleDenominator_ = 1;
  bool planLatched_ = false;
  PlanSource planSource_{};
  GteProjection published_{};
  bool widened_ = false;
  PublicationCensus census_{};
};

} // namespace ctr
