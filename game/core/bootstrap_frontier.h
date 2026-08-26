#pragma once

class Core;

namespace ctr {

class CtrRuntime;

enum class BootstrapResult {
  ReachedSupportedFrontier,
  EntryReturned,
};

// Run the retail startup through every reproducibly verified boundary and stop before the first
// unverified function body at 0x800772E0. This is the product lifecycle boundary, not the trace
// harness.
BootstrapResult runBootstrapToSupportedFrontier(CtrRuntime &runtime, Core &core);

} // namespace ctr
