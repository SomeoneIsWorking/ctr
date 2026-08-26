#pragma once

struct PlatformHlePlan;

namespace ctr {

// Identity-gated retail libgte leaves which record CTR's guest projection at its source boundary.
// This is evidence plumbing, not camera ownership, a native renderer, or a widescreen policy.
const PlatformHlePlan &projectionHlePlan();

} // namespace ctr
