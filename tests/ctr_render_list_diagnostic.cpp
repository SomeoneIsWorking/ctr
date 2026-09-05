#include "core.h"
#include "render_list_boundary_diagnostic.h"

#include <cstdint>
#include <cstdio>
#include <memory>

// Preserved native diagnostic contract: this exercises the production memory watch and source-list
// traversal independently of guest execution. It is not evidence for a rendered frame.
int main() {
  constexpr std::uint32_t kGameState = 0x80040000u;
  constexpr std::uint32_t kList = 0x80050000u;
  constexpr std::uint32_t kFirst = 0x80051000u;
  constexpr std::uint32_t kSecond = 0x80051100u;
  constexpr std::uint32_t kThird = 0x80051200u;
  auto core = std::make_unique<Core>();
  core->r[4] = kGameState;
  core->mem_w32(kGameState + 0x1920u, kFirst);
  core->mem_w32(kGameState + 0x1948u, kThird);
  core->mem_w32(kGameState + 0x1970u, 0u);
  core->mem_w32(kFirst, kSecond);
  core->mem_w32(kSecond, 0u);
  core->mem_w32(kThird, 0u);

  ctr::RenderListBoundaryDiagnostic diagnostic(true);
  diagnostic.observePublication(*core, [&](Core &publisherCore) {
    publisherCore.mem_w32(kGameState + 0x1C94u, kList);
    publisherCore.mem_w32(kFirst + 8u, kList + 8u);
    publisherCore.mem_w32(kSecond + 8u, kList + 8u);
    publisherCore.mem_w32(kThird + 8u, kList + 8u);
  });
  const ctr::RenderListObservation &published = diagnostic.latest();
  if (!published.validGameState || !published.validList || published.list != kList || published.pairStores != 0u ||
      !published.pairWatchArmed || published.sourceLists[0].nodes != 2u || published.sourceLists[0].tail != kSecond ||
      published.sourceLists[0].tailLink != kList + 8u || published.sourceLists[1].nodes != 1u ||
      published.sourceLists[2].nodes != 0u || !published.sourceLists[2].terminated) {
    std::puts("FAIL: render-list publication or source-list ownership changed");
    return 1;
  }

  core->pc = 0x8003B600u;
  core->r[31] = 0x8003B688u;
  core->mem_w32(kList + 4u, 0x12345678u);
  if (diagnostic.latest().pairStores != 1u || !diagnostic.latest().firstWriter.seen ||
      diagnostic.latest().firstWriter.address != kList + 4u || diagnostic.latest().firstWriter.value != 0x12345678u ||
      diagnostic.latest().firstWriter.width != 4u || diagnostic.latest().firstWriter.pc != core->pc ||
      diagnostic.latest().firstWriter.returnAddress != core->r[31]) {
    std::puts("FAIL: production watch did not record the first actual pair store");
    return 1;
  }
  diagnostic.finishField(*core, 7u);
  if (core->storeWatchCb != nullptr) {
    std::puts("FAIL: render-list watch leaked after field completion");
    return 1;
  }
  std::puts("CTR render-list diagnostic: PASS");
  return 0;
}
