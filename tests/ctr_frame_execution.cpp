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
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

constexpr std::uint32_t jump(std::uint32_t address) {
  return 0x08000000u | ((address >> 2u) & 0x03ffffffu);
}

// These synthetic instructions test the title's real continuation owner. They contain no retail
// instruction corpus: the suffix deliberately changes ra, and the next field starts at that ra.
void installFrameProgram(Core &core, std::uint32_t entry) {
  core.imageCatalog().activate("ctr-frame-contract", {0x00010000u, 0x0008d800u}, 1u);
  core.mem_w32(entry, jump(ctr::native::kFrameTiming));
  core.mem_w32(entry + 4u, 0u);
  core.mem_w32(ctr::native::kFrameTiming, 0x2402002au);      // addiu v0, zero, 42
  core.mem_w32(ctr::native::kFrameTiming + 4u, 0x03e00008u); // jr ra
  core.mem_w32(ctr::native::kFrameTiming + 8u, 0u);

  core.mem_w32(ctr::native::kFrameSuffix, 0x3c1f8003u);      // lui ra, 0x8003
  core.mem_w32(ctr::native::kFrameSuffix + 4u, 0x37ffceb4u); // ori ra, ra, 0xceb4
  core.mem_w32(ctr::native::kFrameSuffix + 8u, 0x03e00008u); // jr ra
  core.mem_w32(ctr::native::kFrameSuffix + 12u, 0u);

  core.mem_w32(ctr::native::kFrameLoopResume, 0x26f70001u);      // addiu s7, s7, 1
  core.mem_w32(ctr::native::kFrameLoopResume + 4u, 0x3c1f8003u); // lui ra, 0x8003
  core.mem_w32(ctr::native::kFrameLoopResume + 8u, 0x37ff785cu); // ori ra, ra, 0x785c
  core.mem_w32(ctr::native::kFrameLoopResume + 12u, jump(ctr::native::kFrameTiming));
  core.mem_w32(ctr::native::kFrameLoopResume + 16u, 0u);
}

void installAudioWaitProgram(Core &core) {
  core.r[22] = ctr::native::kStartupAudioWaitState;
  core.mem_w32(ctr::native::kStartupAudioWaitState, 2u);
  core.mem_w32(ctr::native::kFrameLoopResume, 0x3c1f8003u);
  core.mem_w32(ctr::native::kFrameLoopResume + 4u, 0x37ffc94cu);
  core.mem_w32(ctr::native::kFrameLoopResume + 8u, jump(ctr::native::kStartupAudioService));
  core.mem_w32(ctr::native::kFrameLoopResume + 12u, 0u);

  const auto service = ctr::native::kStartupAudioService;
  core.mem_w32(service, 0x8ec80000u);       // lw t0, 0(s6)
  core.mem_w32(service + 4u, 0u);           // load-delay nop
  core.mem_w32(service + 8u, 0x2508ffffu);  // addiu t0, t0, -1
  core.mem_w32(service + 12u, 0xaec80000u); // sw t0, 0(s6)
  core.mem_w32(service + 16u, 0x03e00008u); // jr ra
  core.mem_w32(service + 20u, 0x26b50001u); // addiu s5, s5, 1

  const auto loop = ctr::native::kStartupAudioLoop;
  core.mem_w32(loop, 0x8ec80000u); // lw t0, 0(s6)
  core.mem_w32(loop + 4u, 0u);
  core.mem_w32(loop + 8u, 0x11000005u); // beq t0, zero, loop+32
  core.mem_w32(loop + 12u, 0u);
  core.mem_w32(loop + 16u, 0x3c1f8003u);
  core.mem_w32(loop + 20u, 0x37ffc94cu);
  core.mem_w32(loop + 24u, jump(service));
  core.mem_w32(loop + 28u, 0u);
  core.mem_w32(loop + 32u, 0x3c1f8003u);
  core.mem_w32(loop + 36u, 0x37ff785cu);
  core.mem_w32(loop + 40u, jump(ctr::native::kFrameTiming));
  core.mem_w32(loop + 44u, 0u);
}

} // namespace

int main() {
  constexpr std::uint32_t kEntry = 0x80010000u;
  ctr::CtrRuntime runtime(kEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  installFrameProgram(core, kEntry);
  core.r[28] = 0x00120000u;
  core.r[20] = 0x00130000u;
  core.r[31] = ctr::native::kFrameTimingReturn;
  auto &driver = static_cast<ctr::CtrFrameDriver &>(*game->frameDriver);

  for (std::uint32_t field = 0; field != 3u; ++field) {
    driver.stepFrame(core, field);
    check(driver.completedFrames() == field + 1u, "one field did not complete exactly once");
    check(driver.presentation().completedFences() == field + 1u, "presentation fence diverged from field count");
    check(core.r[23] == field, "driver skipped or executed the next-field continuation early");
    check(core.r[31] == ctr::native::kFrameLoopResume, "suffix did not restore its actual caller");
    check(!core.executionControl().pending(), "field exit leaked into the next host step");
    const auto image = core.currentImageIdentity(ctr::native::kFrameTiming);
    check(image.has_value(), "synthetic image disappeared");
    if (image) {
      check(!core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}),
            "field override leaked after exit");
    }
  }

  installAudioWaitProgram(core);
  driver.stepFrame(core, 3u);
  std::printf("Audio field 3: service_calls=%u wait_state=%u pc=%08x\n",
              core.r[21],
              core.mem_r32(ctr::native::kStartupAudioWaitState),
              core.pc);
  check(core.r[21] == 1u && core.mem_r32(ctr::native::kStartupAudioWaitState) == 1u,
        "audio service did not yield after its first original call");
  driver.stepFrame(core, 4u);
  std::printf("Audio field 4: service_calls=%u wait_state=%u pc=%08x\n",
              core.r[21],
              core.mem_r32(ctr::native::kStartupAudioWaitState),
              core.pc);
  check(core.r[21] == 2u && core.mem_r32(ctr::native::kStartupAudioWaitState) == 0u,
        "resumed original audio loop did not complete its nested service exactly once");
  check(driver.completedFrames() == 5u && driver.presentation().completedFences() == 5u,
        "nested original-loop exit did not finish exactly one field");
  check(!core.executionControl().pending(), "nested original-loop exit leaked into the next field");

  const auto &counters = core.lightrecExecutor().counters();
  check(counters.translatedBlocks > 0u && counters.executedBlocks > 0u, "frame test did not execute translated code");
  check(counters.executedInstructions > 0u, "translated instruction denominator is empty");
  check(counters.fallback.calls == 0u && counters.fallback.instructions == 0u,
        "synthetic frames entered interpretation");
  check(core.mem_r32(core.r[20] + 7388u) == 42u, "native frame bridge lost the original timing result");
  std::printf("CTR translated frame/continuation contract: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
