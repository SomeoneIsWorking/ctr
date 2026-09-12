#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "overlay_image_owner.h"

#include <array>
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

void writeBytes(Core &core, uint32_t destination, std::span<const uint8_t> bytes) {
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    core.mem_w8(destination + static_cast<uint32_t>(index), bytes[index]);
  }
}

} // namespace

int main() {
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  auto otherGame = std::make_unique<Game>();
  Core &core = game->core;
  constexpr uint32_t firstAddress = 0x800A0000u;
  constexpr std::array firstBytes{
      uint8_t{16}, uint8_t{32}, uint8_t{48}, uint8_t{64}, uint8_t{80}, uint8_t{96}, uint8_t{112}, uint8_t{128}};
  constexpr std::array secondBytes{
      uint8_t{1}, uint8_t{2}, uint8_t{3}, uint8_t{4}, uint8_t{5}, uint8_t{6}, uint8_t{7}, uint8_t{8}};
  constexpr std::array descriptors{
      ctr::OverlayDescriptor{"fixture-a", 1u, 10u, 8u, firstAddress, 0x86943430F4D63AA3ull},
      ctr::OverlayDescriptor{"fixture-b", 2u, 20u, 8u, firstAddress + 16u, 0xF2A57B68E1FD4D73ull},
  };
  ctr::OverlayImageOwner images(descriptors, 276u);
  ctr::OverlayImageOwner otherImages(descriptors, 276u);
  constexpr ctr::CompletedDiscRead firstRead{286u, 1u, firstAddress, 0x80u};
  constexpr ctr::CompletedDiscRead secondRead{296u, 1u, firstAddress + 16u, 0x80u};

  writeBytes(core, firstAddress, firstBytes);
  check(images.observeCompletedRead(core, firstRead, true), "exact first read was rejected");
  check(!core.currentImageIdentity(firstAddress), "image became executable before its retail callback");
  const auto firstCandidate = images.takePending();
  check(firstCandidate == 0u, "first read did not name fixture-a");
  check(!core.currentImageIdentity(firstAddress), "taking callback candidate published image early");
  images.publishAfterCallback(core, firstCandidate);
  const auto firstIdentity = core.currentImageIdentity(firstAddress);
  check(firstIdentity.has_value() && images.activeCount() == 1u, "retail callback did not publish fixture-a");
  check(!otherGame->core.currentImageIdentity(firstAddress) && otherImages.activeCount() == 0u,
        "another Core inherited the first Core's overlay image");

  writeBytes(core, secondRead.destination, secondBytes);
  check(images.observeCompletedRead(core, secondRead, true), "exact second read was rejected");
  images.publishAfterCallback(core, images.takePending());
  check(core.currentImageIdentity(firstAddress) == firstIdentity,
        "sector padding made the second image retire the first exact extent");
  check(core.currentImageIdentity(secondRead.destination).has_value() && images.activeCount() == 2u,
        "second image was not published independently");
  check(images.observeCompletedRead(core, {999u, 0u, firstAddress, 0x80u}, false), "zero-sector read was refused");
  check(core.currentImageIdentity(firstAddress) == firstIdentity, "zero-sector read retired an untouched image");

  check(!images.observeCompletedRead(core, {firstRead.lba, firstRead.sectors, firstAddress + 4u, firstRead.mode}, true),
        "known archive read accepted the wrong destination");
  check(!images.observeCompletedRead(core, {firstRead.lba, firstRead.sectors, firstAddress, 0u}, true),
        "known archive read accepted the wrong transfer mode");
  writeBytes(core, firstAddress, firstBytes);
  core.mem_w8(firstAddress + 3u, 0u);
  check(!images.observeCompletedRead(core, firstRead, true), "mutated image bytes were authenticated");
  check(!core.currentImageIdentity(firstAddress), "failed replacement kept a stale active generation");
  check(!images.takePending(), "failed replacement left a pending image candidate");

  writeBytes(core, firstAddress, firstBytes);
  check(images.observeCompletedRead(core, firstRead, true), "reloaded exact image was rejected");
  images.publishAfterCallback(core, images.takePending());
  check(core.currentImageIdentity(firstAddress).has_value() && core.currentImageIdentity(firstAddress) != firstIdentity,
        "reload reused the retired image generation");
  check(images.observeCompletedRead(core, {999u, 1u, firstAddress, 0x80u}, false),
        "unclassified overwrite was refused");
  check(!core.currentImageIdentity(firstAddress), "unclassified overwrite left executable identity active");

  std::printf("CTR BIGFILE image publication: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
