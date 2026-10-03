#include "overlay_image_owner.h"

#include "core.h"
#include "disc.h"
#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <lucent/log.h>
#include <utility>

namespace ctr {
namespace {

constexpr uint32_t kSectorBytes = 2048u;

uint64_t contentIdentity(std::span<const uint8_t> bytes) {
  uint64_t value = 1469598103934665603ull;
  for (const uint8_t byte : bytes) {
    value ^= byte;
    value *= 1099511628211ull;
  }
  return value;
}

bool overlaps(GuestAddressRange lhs, GuestAddressRange rhs) {
  return lhs.begin < rhs.end && rhs.begin < lhs.end;
}

} // namespace

OverlayImageOwner::OverlayImageOwner(std::span<const OverlayDescriptor> descriptors, std::optional<uint32_t> archiveLba)
    : descriptors_(descriptors), archiveLba_(archiveLba), active_(descriptors.size()) {}

bool OverlayImageOwner::locateArchive(Core &core) {
  if (archiveLba_) {
    return true;
  }
  if (!core.game) {
    lucent::error("ctr-overlay", "BIGFILE lookup has no bound Game disc");
    return false;
  }
  uint32_t lba = 0;
  uint32_t bytes = 0;
  if (!disc_find_file(&core.game->disc, "BIGFILE.BIG", &lba, &bytes) || bytes != kBigfileBytes) {
    lucent::error("ctr-overlay", "BIGFILE.BIG is absent or has the wrong byte size (found {})", bytes);
    return false;
  }
  archiveLba_ = lba;
  lucent::info("ctr-overlay", "BIGFILE.BIG located at LBA {} ({} bytes)", lba, bytes);
  return true;
}

void OverlayImageOwner::retireOverwritten(Core &core, GuestAddressRange written) {
  for (std::size_t index = 0; index < active_.size(); ++index) {
    auto &active = active_[index];
    if (!active || (written.valid() && !overlaps(active->range, written))) {
      continue;
    }
    if (!core.imageCatalog().deactivate(active->identity)) {
      lucent::error("ctr-overlay", "active {} generation was already missing", descriptors_[index].name);
      std::abort();
    }
    lucent::info("ctr-overlay",
                 "retired {} generation {} after disc write",
                 descriptors_[index].name,
                 active->identity.generation);
    active.reset();
  }
}

bool OverlayImageOwner::observeCompletedRead(Core &core, CompletedDiscRead read, bool moduleCallbackRegistered) {
  const uint64_t transferBytes = static_cast<uint64_t>(read.sectors) * kSectorBytes;
  const auto written = transferBytes <= std::numeric_limits<uint32_t>::max()
                           ? core.mappedMainRamRange(read.destination, static_cast<uint32_t>(transferBytes))
                           : std::nullopt;
  if (transferBytes != 0u) {
    // An unresolvable nonempty transfer may cross a RAM mirror. Retiring all title generations is
    // conservative; zero sectors wrote nothing and must leave their identities intact.
    retireOverwritten(core, written.value_or(GuestAddressRange{}));
  }

  if (!locateArchive(core)) {
    return false;
  }
  for (std::size_t index = 0; index < descriptors_.size(); ++index) {
    const auto &module = descriptors_[index];
    const uint64_t expectedLba = static_cast<uint64_t>(*archiveLba_) + module.sectorOffset;
    if (read.lba != expectedLba || read.sectors != (module.byteSize + kSectorBytes - 1u) / kSectorBytes) {
      continue;
    }
    if (read.destination != module.loadAddress || read.mode != 0x80u || !moduleCallbackRegistered) {
      lucent::error("ctr-overlay", "{} read used unexpected destination, mode, or callback", module.name);
      return false;
    }
    const auto exact = core.mappedMainRamRange(read.destination, module.byteSize);
    if (!exact) {
      lucent::error("ctr-overlay", "{} read does not fit mapped main RAM", module.name);
      return false;
    }
    const uint64_t actual = contentIdentity({core.ram + exact->begin, module.byteSize});
    if (actual != module.fnv64) {
      lucent::error("ctr-overlay",
                    "{} read content identity {:016X} differs from authenticated {:016X}",
                    module.name,
                    actual,
                    module.fnv64);
      return false;
    }
    if (pending_) {
      lucent::error("ctr-overlay", "{} completed before earlier image callback was delivered", module.name);
      return false;
    }
    pending_ = index;
    lucent::info("ctr-overlay",
                 "authenticated {} read at LBA {} -> 0x{:08X} ({} exact bytes)",
                 module.name,
                 read.lba,
                 module.loadAddress,
                 module.byteSize);
    break;
  }
  return true;
}

std::optional<std::size_t> OverlayImageOwner::takePending() {
  return std::exchange(pending_, std::nullopt);
}

void OverlayImageOwner::publishAfterCallback(Core &core, std::optional<std::size_t> candidate) {
  if (!candidate) {
    return;
  }
  const auto &module = descriptors_[*candidate];
  const auto range = core.mappedMainRamRange(module.loadAddress, module.byteSize);
  if (!range) {
    lucent::error("ctr-overlay", "{} completed callback with no mapped image bytes", module.name);
    std::abort();
  }
  const uint64_t identity = contentIdentity({core.ram + range->begin, module.byteSize});
  if (active_[*candidate]) {
    if (!core.imageCatalog().deactivate(active_[*candidate]->identity)) {
      lucent::error("ctr-overlay", "{} old generation was already missing at publication", module.name);
      std::abort();
    }
  }
  const auto active = core.imageCatalog().activate(module.name, *range, identity);
  active_[*candidate] = ActiveImage{active, *range};
  lucent::info("ctr-overlay",
               "published {} generation {} as [{:08X},{:08X})",
               module.name,
               active.generation,
               range->begin,
               range->end);
}

std::size_t OverlayImageOwner::activeCount() const {
  return std::count_if(active_.begin(), active_.end(), [](const auto &image) {
    return image.has_value();
  });
}

} // namespace ctr
