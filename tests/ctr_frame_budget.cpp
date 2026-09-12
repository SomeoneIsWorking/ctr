#include "core.h"
#include "ctr_runtime.h"
#include "execution_control.h"
#include "frame_driver.h"
#include "game.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "native_ownership.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string_view>

namespace {

inline constexpr std::uint32_t kEntry = 0x80010000u;

constexpr std::uint32_t jump(std::uint32_t address) {
  return 0x08000000u | ((address >> 2u) & 0x03ffffffu);
}

void installFrameBoundary(Core &core) {
  core.mem_w32(ctr::native::kFrameTiming, 0x2402002au);      // addiu v0, zero, 42
  core.mem_w32(ctr::native::kFrameTiming + 4u, 0x03e00008u); // jr ra
  core.mem_w32(ctr::native::kFrameTiming + 8u, 0u);
  core.mem_w32(ctr::native::kFrameSuffix, 0x3c1f8003u);      // lui ra, 0x8003
  core.mem_w32(ctr::native::kFrameSuffix + 4u, 0x37ffceb4u); // ori ra, ra, 0xceb4
  core.mem_w32(ctr::native::kFrameSuffix + 8u, 0x03e00008u); // jr ra
  core.mem_w32(ctr::native::kFrameSuffix + 12u, 0u);
  core.r[28] = 0x00120000u;
  core.r[20] = 0x00130000u;
  core.r[31] = ctr::native::kFrameTimingReturn;
}

void returnToEntry(Core *core) {
  core->r[31] = kEntry;
}

int runFinite(Core &core, ctr::CtrFrameDriver &driver) {
  auto allowance = psx::cpu::ExecutionBudget::currentTurn(core).cycles;
  if (allowance >= std::numeric_limits<std::uint32_t>::max()) {
    std::puts("FAIL: synthetic loop cannot represent the current turn allowance");
    return 1;
  }
  core.r[25] = static_cast<std::uint32_t>(allowance + 1u);
  core.mem_w32(kEntry, 0x2739ffffu);      // addiu t9, t9, -1
  core.mem_w32(kEntry + 4u, 0x1720fffeu); // bne t9, zero, kEntry
  core.mem_w32(kEntry + 8u, 0u);          // delay slot
  core.mem_w32(kEntry + 12u, jump(ctr::native::kFrameTiming));
  core.mem_w32(kEntry + 16u, 0u);
  installFrameBoundary(core);

  driver.stepFrame(core, 0u);
  auto &counters = core.lightrecExecutor().counters();
  bool pass = driver.completedFrames() == 1u && driver.budgetExitsForLastField() > 0u &&
              driver.presentation().completedFences() == 1u && core.game->timing.vblank == 1u && core.r[25] == 0u &&
              !core.executionControl().pending() && counters.executedBlocks > 0u &&
              counters.executedInstructions > 0u && counters.fallback.calls == 0u;
  std::printf("CTR finite budget field: %s budget_exits=%llu executed_blocks=%llu executed_instructions=%llu "
              "fallback_blocks=%llu\n",
              pass ? "PASS" : "FAIL",
              static_cast<unsigned long long>(driver.budgetExitsForLastField()),
              static_cast<unsigned long long>(counters.executedBlocks),
              static_cast<unsigned long long>(counters.executedInstructions),
              static_cast<unsigned long long>(counters.fallback.calls));
  return pass ? 0 : 1;
}

int runStalled(Core &core, ctr::CtrFrameDriver &driver, ctr::CtrRuntime &runtime) {
  core.r[31] = kEntry;
  if (!runtime.installOverride(core, kEntry, "zero-cycle return-to-entry", returnToEntry)) {
    std::puts("FAIL: zero-cycle host loop override was not installed");
    return 1;
  }
  driver.stepFrame(core, 0u);
  std::puts("FAIL: zero-cycle host loop completed a field");
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || (std::string_view(argv[1]) != "finite" && std::string_view(argv[1]) != "stalled")) {
    std::puts("usage: ctr_frame_budget_test {finite|stalled}");
    return 2;
  }
  ctr::CtrRuntime runtime(kEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.imageCatalog().activate("ctr-frame-budget", {0x00010000u, 0x0008d800u}, 1u);
  auto &driver = static_cast<ctr::CtrFrameDriver &>(*game->frameDriver);
  return std::string_view(argv[1]) == "finite" ? runFinite(core, driver) : runStalled(core, driver, runtime);
}
