#pragma once

#include <cstdint>

struct Core;

namespace ctr {

class CtrRuntime;

// Stock Sony libcd ends a CdRead from the CD interrupt: the caller registers a completion function
// through CdReadCallback (0x800771B0, slot 0x8008AD10) and libcd invokes it with CdlComplete.
// psxport transfers every requested sector before the native CdRead leaf returns, so that interrupt
// never happens, and a caller which waits for the callback instead of polling CdReadSync waits
// forever.
//
// MEASURED on SCUS_944.26: the resource loader at 0x800321B4 registers 0x80032110 and the slot is
// never written again, so its busy flag at gp+0x138 stays 1, the screen loader at 0x80033610 keeps
// returning stage 2 unchanged, the ordering table stays empty, and every presented field is black.
//
// This owner runs the shared native transfer and then dispatches the exact registered guest
// callback. It transcribes none of that callback's effects: the retail body owns them.
class DiscReadOwner final {
public:
  // Delivers the registered libcd completion callback with the measured success code, restoring the
  // interrupted register context exactly as a real CD interrupt entry would.
  void deliverCompletion(Core &core, const CtrRuntime &runtime);

  [[nodiscard]] uint32_t deliveredCallbacks() const {
    return deliveredCallbacks_;
  }
  [[nodiscard]] uint32_t polledReads() const {
    return polledReads_;
  }

private:
  uint32_t deliveredCallbacks_ = 0;
  uint32_t polledReads_ = 0;
};

// PlatformHle binding for the measured stock libcd CdRead leaf: the shared synchronous transfer
// followed by the retail completion callback.
void cdReadWithCompletionCallback(Core *core);

} // namespace ctr
