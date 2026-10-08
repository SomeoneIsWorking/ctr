// Frame-driver execution budget against hand-assembled guest code.

#include "async_disc_owner.h"
#include "cd_drive_timing.h"
#include "core.h"
#include "ctr_runtime.h"
#include "execution_control.h"
#include "frame_driver.h"
#include "game.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "native_ownership.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#define CTR_FRAME_BUDGET_HAS_FORK 1
#else
#define CTR_FRAME_BUDGET_HAS_FORK 0
#endif

namespace {

inline constexpr std::uint32_t kEntry = 0x80010000u;
inline constexpr std::uint32_t kReadStart = 0x80010100u;
inline constexpr std::uint32_t kCompletionCallback = 0x80010200u;
inline constexpr std::uint32_t kDeliveredWord = 0x80010300u;

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

constexpr std::uint32_t call(std::uint32_t address) {
  return 0x0C000000u | ((address >> 2u) & 0x03ffffffu);
}

// A spin of `extra` iterations beyond one turn plus one cycle, so a field must take a budget exit before the boundary.
bool writeSpinLoop(Core &core, std::uint32_t at, std::uint64_t extra = 0) {
  auto allowance = psx::cpu::ExecutionBudget::currentTurn(core).cycles;
  if (allowance + extra >= std::numeric_limits<std::uint32_t>::max()) {
    std::puts("FAIL: synthetic loop cannot represent the current turn allowance");
    return false;
  }
  core.r[25] = static_cast<std::uint32_t>(allowance + extra + 1u);
  core.mem_w32(at, 0x2739ffffu);      // addiu t9, t9, -1
  core.mem_w32(at + 4u, 0x1720fffeu); // bne t9, zero, at
  core.mem_w32(at + 8u, 0u);          // delay slot
  core.mem_w32(at + 12u, jump(ctr::native::kFrameTiming));
  core.mem_w32(at + 16u, 0u);
  return true;
}

void returnToEntry(Core *core) {
  core->r[31] = kEntry;
}

int runFinite(Core &core, ctr::CtrFrameDriver &driver) {
  if (!writeSpinLoop(core, kEntry)) {
    return 1;
  }
  installFrameBoundary(core);

  driver.stepFrame(core, 0u);
  auto &counters = core.lightrecExecutor().counters();
  bool pass = driver.completedFrames() == 1u && driver.budgetExitsForLastField() > 0u &&
              driver.presentation().completedFences() == 1u && core.game->gpu.gpu_frame_no() == 1 &&
              core.game->timing.vblank == 1u && core.r[25] == 0u && !core.executionControl().pending() &&
              counters.executedBlocks > 0u && counters.executedInstructions > 0u && counters.fallback.calls == 0u;
  std::printf("CTR finite budget field: %s budget_exits=%llu executed_blocks=%llu executed_instructions=%llu "
              "fallback_blocks=%llu\n",
              pass ? "PASS" : "FAIL",
              static_cast<unsigned long long>(driver.budgetExitsForLastField()),
              static_cast<unsigned long long>(counters.executedBlocks),
              static_cast<unsigned long long>(counters.executedInstructions),
              static_cast<unsigned long long>(counters.fallback.calls));
  return pass ? 0 : 1;
}

std::uint32_t readSectors = 1;

void startRead(Core *core) {
  ctr::discReadOwner(*core).noteTransferComplete(*core, std::nullopt, readSectors, 0x80u);
}

// A read finishes inside the field and the guest then spins on its callback, never reaching a field start.
// The callback may arrive only once the drive would have spent the read's time.
int runCompletion(Core &core, ctr::CtrFrameDriver &driver, std::uint32_t sectors, bool expectDelivered) {
  readSectors = sectors;
  core.mem_w32(kEntry, call(kReadStart));
  core.mem_w32(kEntry + 4u, 0u);
  core.mem_w32(kEntry + 8u, 0x3C1F8003u);  // lui ra, 0x8003
  core.mem_w32(kEntry + 12u, 0x37FF785Cu); // ori ra, ra, 0x785c: kFrameTimingReturn, clobbered by the call
  if (!writeSpinLoop(core, kEntry + 16u, 4u * cd_drive_sector_period_cpu_ticks(0x80u))) {
    return 1;
  }
  core.mem_w32(kCompletionCallback, 0x3C088001u);       // lui t0, 0x8001
  core.mem_w32(kCompletionCallback + 4u, 0x24020001u);  // addiu v0, zero, 1
  core.mem_w32(kCompletionCallback + 8u, 0x03E00008u);  // jr ra
  core.mem_w32(kCompletionCallback + 12u, 0xAD020300u); // sw v0, 0x300(t0): kDeliveredWord
  core.mem_w32(ctr::native::kCdReadCompletionCallback, kCompletionCallback);
  installFrameBoundary(core);
  if (!psx::cpu::tryInstallNativeOverride(core, kReadStart, "synthetic CdRead completion", startRead).has_value()) {
    std::puts("FAIL: synthetic read override was not installed");
    return 1;
  }

  driver.stepFrame(core, 0u);
  auto &owner = driver.discReadOwner();
  const bool delivered =
      owner.deliveredCallbacks() == 1u && !owner.hasPendingCompletion() && core.mem_r32(kDeliveredWord) == 1u;
  const bool owed =
      owner.deliveredCallbacks() == 0u && owner.hasPendingCompletion() && core.mem_r32(kDeliveredWord) == 0u;
  const bool pass = driver.completedFrames() == 1u && (expectDelivered ? delivered : owed);
  std::printf("CTR %u-sector read completion inside a spinning field: %s delivered=%u\n",
              sectors,
              pass ? "PASS" : "FAIL",
              owner.deliveredCallbacks());
  return pass ? 0 : 1;
}

int runStalled(Core &core, ctr::CtrFrameDriver &driver, ctr::CtrRuntime &runtime) {
  core.r[31] = kEntry;
  if (!psx::cpu::tryInstallNativeOverride(core, kEntry, "zero-cycle return-to-entry", returnToEntry).has_value()) {
    std::puts("FAIL: zero-cycle host loop override was not installed");
    return 1;
  }
  driver.stepFrame(core, 0u);
  std::puts("FAIL: zero-cycle host loop completed a field");
  return 1;
}

#if CTR_FRAME_BUDGET_HAS_FORK
int runStalledExpectingRefusal() {
  int channel[2] = {-1, -1};
  if (::pipe(channel) != 0) {
    std::puts("FAIL: could not create the refusal pipe");
    return 1;
  }
  const ::pid_t child = ::fork();
  if (child < 0) {
    std::puts("FAIL: could not fork the refusing child");
    return 1;
  }
  if (child == 0) {
    ::close(channel[0]);
    ::dup2(channel[1], STDOUT_FILENO);
    ::dup2(channel[1], STDERR_FILENO);
    ::close(channel[1]);
    ctr::CtrRuntime runtime(kEntry);
    psxport_install_game(runtime);
    auto game = std::make_unique<Game>();
    Core &core = game->core;
    core.imageCatalog().activate("ctr-frame-budget", {0x00010000u, 0x0008d800u}, 1u);
    auto &driver = static_cast<ctr::CtrFrameDriver &>(*game->frameDriver);
    ::_exit(runStalled(core, driver, runtime));
  }

  ::close(channel[1]);
  std::string captured;
  char buffer[512];
  for (;;) {
    const ::ssize_t read_count = ::read(channel[0], buffer, sizeof buffer);
    if (read_count <= 0) {
      break;
    }
    captured.append(buffer, static_cast<std::size_t>(read_count));
  }
  ::close(channel[0]);

  int status = 0;
  if (::waitpid(child, &status, 0) != child) {
    std::puts("FAIL: the refusing child was not reaped");
    return 1;
  }
  if (!WIFSIGNALED(status)) {
    std::printf("FAIL: zero-cycle host loop did not terminate the child (status 0x%X)\n", status);
    return 1;
  }
  if (captured.find("zero-cycle host loop completed a field") != std::string::npos) {
    std::puts("FAIL: zero-cycle host loop completed a field");
    return 1;
  }
  std::printf("CTR zero-cycle host loop: refused by the driver (child signal %d)\n", WTERMSIG(status));
  return 0;
}
#else
int runStalledExpectingRefusal() {
  std::puts("SKIP: this host has no fork, and the refusal is observable only across a process boundary");
  return 77;
}
#endif

} // namespace

int main(int argc, char **argv) {
  const std::string_view mode = argc == 2 ? argv[1] : "";
  if (mode != "finite" && mode != "stalled" && mode != "completion" && mode != "completion_not_due") {
    std::puts("usage: ctr_frame_budget_test {finite|stalled|completion|completion_not_due}");
    return 2;
  }
  if (std::string_view(argv[1]) == "stalled") {
    return runStalledExpectingRefusal();
  }
  ctr::CtrRuntime runtime(kEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.imageCatalog().activate("ctr-frame-budget", {0x00010000u, 0x0008d800u}, 1u);
  auto &driver = static_cast<ctr::CtrFrameDriver &>(*game->frameDriver);
  if (mode == "completion") {
    return runCompletion(core, driver, 1u, true);
  }
  if (mode == "completion_not_due") {
    return runCompletion(core, driver, 4000u, false);
  }
  return runFinite(core, driver);
}
