#pragma once

struct Core;

namespace ctr {

class CtrRuntime;

// Adapts CTR's interrupt-completed disc wrapper to psxport's synchronous native CD transfer.
// The generated wrapper remains authoritative for sector calculation and the transfer itself.
class AsyncDiscOwner final {
public:
  void startRead(Core &core, const CtrRuntime &runtime) const;
};

} // namespace ctr
