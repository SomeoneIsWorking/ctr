#pragma once

struct PlatformHlePlan;

namespace ctr {

// Identity-gated retail platform leaves. Projection recording is evidence plumbing; VSync is the
// mandatory fatal ownership trap which prevents the guest from becoming a second frame-loop owner.
const PlatformHlePlan &platformHlePlan();

} // namespace ctr
