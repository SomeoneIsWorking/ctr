#pragma once

#include "overlay_image_owner.h"

#include <cstdint>
#include <optional>

struct Core;

namespace ctr {

class CtrRuntime;

// Stock libcd ends CdRead from the CD interrupt via the callback at 0x800771B0 (slot 0x8008AD10);
// psxport transfers synchronously, so that completion is owed until the issuing call unwinds.
class DiscReadOwner final {
public:
  // `sectors` at CdRead `mode` set how long the drive owes before the completion may interrupt mid-field.
  void noteTransferComplete(Core &core,
                            std::optional<CompletedDiscRead> read = std::nullopt,
                            uint32_t sectors = 0,
                            uint8_t mode = 0x80u);

  void deliverPending(Core &core, const CtrRuntime &runtime);

  // Mid-field delivery for a guest that polls for its completion: the drive's read time must have elapsed,
  // so the loader that issued the read has published its state first.
  void deliverIfDue(Core &core, const CtrRuntime &runtime);

  [[nodiscard]] bool hasPendingCompletion() const {
    return pending_;
  }
  [[nodiscard]] uint32_t deliveredCallbacks() const {
    return deliveredCallbacks_;
  }
  [[nodiscard]] uint32_t polledReads() const {
    return polledReads_;
  }

private:
  OverlayImageOwner overlayImages_;
  bool pending_ = false;
  uint64_t dueTicks_ = 0;
  uint32_t deliveredCallbacks_ = 0;
  uint32_t polledReads_ = 0;
};

DiscReadOwner &discReadOwner(Core &core);

// PlatformHle binding for the stock libcd CdRead leaf: shared synchronous transfer, then an owed callback.
void cdReadWithCompletionCallback(Core *core);

} // namespace ctr
