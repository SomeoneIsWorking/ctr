#pragma once

#include "guest_address_range.h"
#include "identity/overlay_catalog.h"
#include "image_identity.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

class Core;

namespace ctr {

struct CompletedDiscRead {
  uint32_t lba = 0;
  uint32_t sectors = 0;
  uint32_t destination = 0;
  uint32_t mode = 0;
};

// A BIGFILE transfer becomes an image only once its completion callback has relocated it.
class OverlayImageOwner final {
public:
  explicit OverlayImageOwner(std::span<const OverlayDescriptor> descriptors = kOverlayDescriptors,
                             std::optional<uint32_t> archiveLba = std::nullopt);

  bool observeCompletedRead(Core &core, CompletedDiscRead read, bool callbackRegistered);
  [[nodiscard]] std::optional<std::size_t> takePending();
  void publishAfterCallback(Core &core, std::optional<std::size_t> candidate);

  [[nodiscard]] std::size_t activeCount() const;

private:
  struct ActiveImage {
    psx::cpu::ImageIdentity identity;
    GuestAddressRange range;
  };

  // The BIGFILE.BIG start sector, found once; empty when the disc has no matching archive.
  std::optional<uint32_t> locateArchive(Core &core);
  void retireOverwritten(Core &core, GuestAddressRange written);

  std::span<const OverlayDescriptor> descriptors_;
  std::optional<uint32_t> archiveLba_;
  std::vector<std::optional<ActiveImage>> active_;
  std::optional<std::size_t> pending_;
};

} // namespace ctr
