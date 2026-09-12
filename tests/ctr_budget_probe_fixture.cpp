#include "core.h"
#include "ctr_runtime.h"
#include "execution_exit.h"
#include "game.h"
#include "image_identity.h"
#include "lightrec_executor.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>

namespace ctr {
namespace {

inline constexpr std::uint32_t kLoop = 0x8006A57Cu;
inline constexpr std::uint32_t kBfBegin = 0x000AB9F0u;
inline constexpr std::uint32_t kBfEnd = 0x000B97FCu;
inline constexpr std::uint32_t kPrologue = 0x800ABDE0u;
inline constexpr std::uint32_t kPrologueWord = 0xAFB3002Cu;

constexpr std::uint32_t jump(std::uint32_t address) {
  return 0x08000000u | ((address >> 2u) & 0x03ffffffu);
}

} // namespace

// GDB stops at this named seam after a real bounded Lightrec dispatch. The function performs no
// continuation itself; the diagnostic invokes the same CtrRuntime::dispatch used by the product.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void budgetProbeFixtureStop(Core &core, CtrRuntime &runtime, const psx::cpu::ExecutionResult &first) {
  std::printf("CTR_BUDGET_FIXTURE_READY entry=%08x pc=%08x cycles=%llu reason=%u t9=%08x t3=%08x\n",
              runtime.bootTarget(),
              core.pc,
              static_cast<unsigned long long>(first.cycles),
              static_cast<unsigned>(first.reason),
              core.r[25],
              core.r[11]);
}

} // namespace ctr

int main(int argc, char **argv) {
  if (argc != 2) {
    std::puts("usage: ctr_budget_probe_fixture {advance|stagnant|missing-image|missing-trigger}");
    return 2;
  }
  std::string_view mode = argv[1];
  if (mode != "advance" && mode != "stagnant" && mode != "missing-image" && mode != "missing-trigger") {
    std::puts("usage: ctr_budget_probe_fixture {advance|stagnant|missing-image|missing-trigger}");
    return 2;
  }
  ctr::CtrRuntime runtime(ctr::kLoop);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.imageCatalog().activate("ctr-budget-fixture", {0x00010000u, 0x0008D800u}, 1u);
  if (mode != "missing-image") {
    core.imageCatalog().activate("BF0233", {ctr::kBfBegin, ctr::kBfEnd}, 2u);
  }
  core.mem_w32(ctr::kPrologue, ctr::kPrologueWord);
  core.r[25] = 0x80100000u;
  core.r[11] = 0x12340000u;
  core.mem_w32(ctr::kLoop, mode == "advance" ? 0x27390004u : 0u);
  core.mem_w32(ctr::kLoop + 4u, ctr::jump(ctr::kLoop));
  core.mem_w32(ctr::kLoop + 8u, mode == "advance" ? 0x256B0001u : 0u);
  auto first = core.lightrecExecutor().executeUntilExit(ctr::kLoop, psx::cpu::ExecutionBudget::fromCycles(64u));
  if (first.reason != psx::cpu::ExecutionExitReason::BudgetExhausted || first.guestPc != core.pc ||
      first.guestPc != ctr::kLoop) {
    std::printf("FAIL: initial synthetic Lightrec budget pc=%08x reason=%u\n",
                first.guestPc,
                static_cast<unsigned>(first.reason));
    return 1;
  }
  if (mode != "missing-trigger") {
    ctr::budgetProbeFixtureStop(core, runtime, first);
  }
  return 0;
}
