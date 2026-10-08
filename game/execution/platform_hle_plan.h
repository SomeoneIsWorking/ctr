#pragma once

struct PlatformHlePlan;

namespace ctr {

// Retail platform leaves; VSync is a fatal trap so the guest cannot become a second frame-loop owner.
const PlatformHlePlan &platformHlePlan();

} // namespace ctr
