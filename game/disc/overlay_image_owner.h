#pragma once

#include "ctr_overlay_catalog.h"
#include "guest_program_image.h"
#include "image_identity.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct Core;

namespace ctr {

struct CompletedDiscRead {
  uint32_t lba = 0;
  uint32_t sectors = 0;
  uint32_t destination = 0;
  uint32_t mode = 0;
};

// A BIGFILE sector transfer is only a candidate image. Retail's completion callback relocates
// and publishes it; this owner activates that exact RAM extent after the callback returns.
class OverlayImageOwner final {
public:
  explicit OverlayImageOwner(std::span<const OverlayDescriptor> descriptors = kOverlayDescriptors,
                             std::optional<uint32_t> archiveLba = std::nullopt);

  // False means an exact title image read had the wrong bytes or an invalid destination. The
  // caller stops; it must never dispatch through a guessed image identity.
  bool observeCompletedRead(Core &core, CompletedDiscRead read, bool callbackRegistered);
  [[nodiscard]] std::optional<std::size_t> takePending();
  void publishAfterCallback(Core &core, std::optional<std::size_t> candidate);

  [[nodiscard]] std::size_t activeCount() const;

private:
  struct ActiveImage {
    psx::cpu::ImageIdentity identity;
    GuestAddressRange range;
  };

  bool locateArchive(Core &core);
  void retireOverwritten(Core &core, GuestAddressRange written);

  std::span<const OverlayDescriptor> descriptors_;
  std::optional<uint32_t> archiveLba_;
  std::vector<std::optional<ActiveImage>> active_;
  std::optional<std::size_t> pending_;
};

} // namespace ctr
