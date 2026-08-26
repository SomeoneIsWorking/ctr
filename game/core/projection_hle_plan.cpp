#include "projection_hle_plan.h"

#include "platform_hle.h"

namespace ctr {
namespace {

// tools/measure_render_frontier.py refuses unless both complete retail leaf bodies, their callers,
// and the complete CR24/CR25/CR26 text-word census match the selected SCUS_944.26 executable. The
// half-open window admits exactly SetGeomScreen [0x8007781C,0x80077828) plus SetGeomOffset
// [0x8007782C,0x80077844); the four-byte gap remains unregistered.
const PlatformHlePlan kProjectionHlePlan{
    .setGeomOffset = 0x8007782Cu,
    .setGeomScreen = 0x8007781Cu,
    .bindings = {},
    .bindingCount = 0,
    .windowLo = {0x8007781Cu, 0u},
    .windowHi = {0x80077844u, 0u},
};

} // namespace

const PlatformHlePlan &projectionHlePlan() {
  return kProjectionHlePlan;
}

} // namespace ctr
