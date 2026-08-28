#include "async_disc_owner.h"
#include "ctr_runtime.h"
#include "dma_callback_owner.h"

#include "core.h"
#include "frame_callback_owner.h"
#include "frame_driver.h"
#include "frame_loop_shell.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "native_ownership.h"
#include "platform_hle.h"
#include "runtime_composition.h"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

namespace {

Core *g_dispatchedCore = nullptr;
constexpr uint32_t kFakeView = 0x00040000u;
constexpr uint32_t kRetailReadCompletionCallback = 0x80032110u;
constexpr std::array<uint32_t, 10> kOverrideAddresses{
    ctr::native::kStartupGpuInit,
    ctr::native::kStartupDisplayInit,
    ctr::native::kBootResourceWait,
    ctr::native::kBootResourcePump,
    ctr::native::kStartupAudioService,
    ctr::native::kStartupAudioLoop,
    ctr::native::kShutdownDisplay,
    ctr::native::kVblankCallbackInstall,
    ctr::native::kFrameTiming,
    ctr::native::kProjectionProducer,
};
std::array<ctr::CtrRuntime::RecompiledOverride, kOverrideAddresses.size()> g_overrides{};
std::array<uint32_t, 32> g_dispatchTrace{};
std::array<uint32_t, 32> g_dispatchRaTrace{};
std::array<uint32_t, 32> g_dispatchA0Trace{};
std::array<uint32_t, 32> g_superTrace{};
std::size_t g_dispatchCount = 0;
std::size_t g_superCount = 0;
bool g_routeShutdownOnResume = false;
bool g_routeBootResourcePumpOnResume = false;
bool g_routeBootResourceRaceWaitOnResume = false;
bool g_routeStartupAudioWaitOnResume = false;
uint32_t g_bootResourcePolls = 0;
uint32_t g_bootResourceCommitPolls = 0;
uint32_t g_startupAudioServiceCalls = 0;
bool g_dmaCompletionOwed = false;
int g_dmaTakenChannel = -1;
int g_dmaAckedChannel = -1;
uint32_t g_dmaDispatchAddress = 0;
bool g_dmaDispatchInIrq = false;
bool g_chainDmaDuringDispatch = false;
std::array<uint32_t, 2> g_frameCallbackTrace{};
std::size_t g_frameCallbackCount = 0;
bool g_frameCallbackInIrq = false;

bool fakeDmaOwed(int channel) {
  return channel == ctr::native::kSpuDmaChannel && g_dmaCompletionOwed;
}

void fakeDmaTake(int channel) {
  g_dmaTakenChannel = channel;
  g_dmaCompletionOwed = false;
}

void fakeDmaAck(int channel) {
  g_dmaAckedChannel = channel;
}

void captureDmaDispatch(Core *core, uint32_t address) {
  g_dmaDispatchAddress = address;
  g_dmaDispatchInIrq = core->game != nullptr && core->game->hle.in_irq != 0;
  if (g_chainDmaDuringDispatch) {
    g_dmaCompletionOwed = true;
  }
  core->r[2] = 0xFFFFFFFFu;
  core->r[31] = 0xFFFFFFFFu;
}

void captureFrameCallbackDispatch(Core *core, uint32_t address) {
  g_frameCallbackTrace[g_frameCallbackCount++] = address;
  g_frameCallbackInIrq = g_frameCallbackInIrq || (core->game != nullptr && core->game->hle.in_irq != 0);
  const uint32_t gameState = core->mem_r32(core->r[28] + ctr::native::kGameStateGpOffset);
  if (address == 0x80034A80u) {
    core->mem_w8(gameState + ctr::native::kDrawSyncPendingOffset, 0u);
  } else if (address == 0x80034AA4u) {
    const uint32_t waitFields = core->mem_r32(core->r[28] + ctr::native::kFrameWaitFieldsGpOffset);
    core->mem_w32(core->r[28] + ctr::native::kFrameWaitFieldsGpOffset, waitFields - 1u);
    const uint32_t count = core->mem_r32(gameState + ctr::native::kFrameCallbackCountOffset);
    core->mem_w32(gameState + ctr::native::kFrameCallbackCountOffset, count + 1u);
  } else {
    std::abort();
  }
  core->r[2] = 0xFFFFFFFFu;
  core->r[31] = 0xFFFFFFFFu;
}

ctr::CtrRuntime::RecompiledOverride lookupOverride(uint32_t address) {
  for (std::size_t index = 0; index < kOverrideAddresses.size(); ++index) {
    if (kOverrideAddresses[index] == address) {
      return g_overrides[index];
    }
  }
  return nullptr;
}

void captureOverride(uint32_t address, ctr::CtrRuntime::RecompiledOverride overrideFunction) {
  for (std::size_t index = 0; index < kOverrideAddresses.size(); ++index) {
    if (kOverrideAddresses[index] == address) {
      g_overrides[index] = overrideFunction;
      return;
    }
  }
  std::abort();
}

void captureDispatch(Core *core, uint32_t address) {
  g_dispatchedCore = core;
  g_dispatchTrace[g_dispatchCount] = address;
  g_dispatchRaTrace[g_dispatchCount] = core->r[31];
  g_dispatchA0Trace[g_dispatchCount] = core->r[4];
  ++g_dispatchCount;

  auto invoke = [&](uint32_t overrideAddress, uint32_t returnAddress) {
    ctr::CtrRuntime::RecompiledOverride function = lookupOverride(overrideAddress);
    if (!function) {
      std::abort();
    }
    core->r[31] = returnAddress;
    function(core);
  };
  auto finishFrame = [&] {
    core->r[4] = kFakeView;
    invoke(ctr::native::kProjectionProducer, ctr::native::kProjectionReturnState);
    invoke(ctr::native::kFrameTiming, ctr::native::kFrameTimingReturn);
  };

  switch (address) {
  case ctr::native::kExecutableEntry:
    invoke(ctr::native::kStartupGpuInit, ctr::native::kStartupGpuInitReturn);
    return;
  case ctr::native::kAfterFirstStartupVSync:
    invoke(ctr::native::kStartupDisplayInit, ctr::native::kStartupDisplayInitReturn);
    return;
  case ctr::native::kAfterSecondStartupVSync:
  case ctr::native::kAfterShutdownVSync:
    finishFrame();
    return;
  case ctr::native::kFrameLoopResume:
    if (g_routeBootResourceRaceWaitOnResume) {
      g_routeBootResourceRaceWaitOnResume = false;
      invoke(ctr::native::kBootResourceWait, ctr::native::kBootResourceWaitRaceCaller);
    } else if (g_routeStartupAudioWaitOnResume) {
      g_routeStartupAudioWaitOnResume = false;
      core->mem_w32(ctr::native::kStartupAudioWaitState, 1u);
      invoke(ctr::native::kStartupAudioService, ctr::native::kStartupAudioServiceReturn);
    } else if (g_routeBootResourcePumpOnResume) {
      g_routeBootResourcePumpOnResume = false;
      invoke(ctr::native::kBootResourcePump, ctr::native::kBootResourcePumpReturn);
    } else if (g_routeShutdownOnResume) {
      g_routeShutdownOnResume = false;
      invoke(ctr::native::kShutdownDisplay, ctr::native::kShutdownDisplayReturn);
    } else {
      finishFrame();
    }
    return;
  case ctr::native::kBootResourcePumpBegin:
  case ctr::native::kBootResourcePumpCommit:
    return;
  case 0x800321B4u:
    core->r[2] = 0x00ABCDEFu;
    return;
  // The measured retail read-completion callback: its first act is CdReadCallback(0). Clobbering
  // v0/ra here is what proves the owner restores the interrupted context.
  case kRetailReadCompletionCallback:
    core->mem_w32(ctr::native::kCdReadCompletionCallback, 0u);
    core->r[2] = 0u;
    core->r[31] = 0u;
    return;
  case 0x8003E978u:
    core->r[2] = 0u;
    return;
  case 0x80031EE4u:
    return;
  case ctr::native::kBootResourceWaitReturn:
    core->r[31] = core->mem_r32(core->r[29] + 68u);
    core->r[20] = core->mem_r32(core->r[29] + 64u);
    core->r[19] = core->mem_r32(core->r[29] + 60u);
    core->r[18] = core->mem_r32(core->r[29] + 56u);
    core->r[17] = core->mem_r32(core->r[29] + 52u);
    core->r[16] = core->mem_r32(core->r[29] + 48u);
    core->r[29] += 72u;
    return;
  case ctr::native::kBootResourceWaitRaceCaller:
    core->r[31] = ctr::native::kBootResourceWaitRaceResume;
    return;
  case ctr::native::kBootResourceWaitRaceResume:
    invoke(ctr::native::kFrameTiming, ctr::native::kFrameTimingQueryReturn);
    finishFrame();
    return;
  case ctr::native::kBootResourcePumpPoll:
    core->r[2] = ++g_bootResourcePolls < 2u ? 0u : 1u;
    return;
  case ctr::native::kBootResourcePumpCommitPoll:
    core->r[2] = ++g_bootResourceCommitPolls < 2u ? 0u : 1u;
    return;
  case ctr::native::kBootResourcePumpReturn:
    finishFrame();
    return;
  case ctr::native::kStartupAudioServiceReturn:
    invoke(ctr::native::kStartupAudioLoop, core->r[31]);
    return;
  case ctr::native::kFrameSuffix:
    core->r[31] = ctr::native::kFrameLoopResume;
    return;
  default:
    std::abort();
  }
}

void captureSuper(Core *core, uint32_t address) {
  g_superTrace[g_superCount++] = address;
  if (address == ctr::native::kFrameTiming) {
    core->r[2] = 0x00C0FFEEu;
  } else if (address == ctr::native::kProjectionProducer) {
    core->rsub.projParams.setGeomOffset(160.0f, 120.0f);
    core->rsub.projParams.setGeomScreen(512.0f);
  } else if (address == ctr::native::kStartupAudioService) {
    if (++g_startupAudioServiceCalls == 2u) {
      core->mem_w32(ctr::native::kStartupAudioWaitState, 0u);
    }
  } else if (address == ctr::native::kStartupAudioLoop) {
    core->r[31] = ctr::native::kStartupAudioServiceReturn;
    lookupOverride(ctr::native::kStartupAudioService)(core);
    if (core->mem_r32(ctr::native::kStartupAudioWaitState) == 0u) {
      core->r[4] = kFakeView;
      core->r[31] = ctr::native::kProjectionReturnState;
      lookupOverride(ctr::native::kProjectionProducer)(core);
      core->r[31] = ctr::native::kFrameTimingReturn;
      lookupOverride(ctr::native::kFrameTiming)(core);
    }
  }
}

struct VSyncCall {
  Game *game;
  int32_t mode;
};

void invokeVSync(void *context) {
  auto *call = static_cast<VSyncCall *>(context);
  const OverrideFn handler = call->game->platform_hle.lookup(ctr::native::kVSync);
  if (!handler) {
    _exit(3);
  }
  call->game->core.r[4] = static_cast<uint32_t>(call->mode);
  handler(&call->game->core);
}

struct GpuTimeoutCall {
  Game *game;
};

void invokeExpiredGpuTimeout(void *context) {
  auto *call = static_cast<GpuTimeoutCall *>(context);
  Core &core = call->game->core;
  core.mem_w32(ctr::native::kGpuTimeoutDeadline, call->game->timing.vblank - 1u);
  core.mem_w32(ctr::native::kGpuTimeoutPollCount, 0u);
  call->game->platform_hle.lookup(ctr::native::kGpuTimeoutCheck)(&core);
}

bool operationAborts(void (*operation)(void *), void *context) {
  const pid_t child = fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    operation(context);
    _exit(0);
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

struct ProjectionMismatchCall {
  Core *core;
};

void invokeProjectionMismatch(void *context) {
  auto *call = static_cast<ProjectionMismatchCall *>(context);
  ctr::ProjectionOwner owner;
  call->core->r[4] = kFakeView;
  owner.publish(*call->core, [](Core &projectionCore) {
    projectionCore.rsub.projParams.setGeomOffset(0.0f, 0.0f);
    projectionCore.rsub.projParams.setGeomScreen(1.0f);
  });
}

} // namespace

int main() {
  static_assert(std::is_base_of_v<GameRuntime, ctr::CtrRuntime>);
  static_assert(ctr::native::kGuestMain == 0x8003C58Cu);
  static_assert(ctr::native::kLoopTop == 0x8003C5D0u);

  ctr::CtrRuntime runtime(captureDispatch, ctr::native::kExecutableEntry, captureOverride, captureSuper);
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  Core &core = game->core;
  if (psxport_game_runtime() != &runtime || core.runtime != &runtime ||
      dynamic_cast<ctr::CtrFrameDriver *>(game->frameDriver.get()) == nullptr) {
    std::fprintf(stderr, "production runtime did not create CTR's title frame driver\n");
    return 1;
  }
  if (runtime.legacyConfigForMigration() != nullptr || runtime.legacyHooksForMigration() != nullptr ||
      core.cfg != nullptr || core.hooks != nullptr) {
    std::fprintf(stderr, "CtrRuntime exposed a legacy GameConfig/GameHooks view\n");
    return 1;
  }

  const GuestProgramImage *programImage = runtime.guestProgramImage();
  if (programImage == nullptr || core.guestProgramImage != programImage ||
      programImage->residentText.begin != 0x00010000u || programImage->residentText.end != 0x0008D800u ||
      !programImage->residentText.containsPhysical(0x80010000u) ||
      !programImage->residentText.containsPhysical(0x8008D7FFu) ||
      programImage->residentText.containsPhysical(0x8008D800u)) {
    std::fprintf(stderr, "CtrRuntime lost its measured resident program range\n");
    return 1;
  }
  if (core.gameCtx != nullptr || runtime.bootTarget() != ctr::native::kExecutableEntry) {
    std::fprintf(stderr, "CtrRuntime invented context state or lost its measured entry\n");
    return 1;
  }
  const RenderCapabilities renderCapabilities = runtime.renderCapabilities();
  if (renderCapabilities.defaultPath != RenderPath::Gte || renderCapabilities.nativeRenderPath ||
      renderCapabilities.temporalInterpolation || renderCapabilities.playerPathCount() != 1) {
    std::fprintf(stderr, "CtrRuntime exposed unimplemented native or temporal player controls\n");
    return 1;
  }

  const PlatformHlePlan *platformPlan = runtime.platformHlePlan();
  if (platformPlan == nullptr || platformPlan->setGeomScreen != ctr::native::kSetGeomScreen ||
      platformPlan->setGeomOffset != ctr::native::kSetGeomOffset || platformPlan->vsyncAddress != ctr::native::kVSync ||
      platformPlan->bindingCount != 6 || platformPlan->bindings[0].addr != ctr::native::kGpuTimeoutArm ||
      platformPlan->bindings[1].addr != ctr::native::kGpuTimeoutCheck ||
      platformPlan->bindings[2].addr != ctr::native::kCdRead ||
      platformPlan->bindings[3].addr != ctr::native::kCdReadSync ||
      platformPlan->bindings[4].addr != ctr::native::kCdSync ||
      platformPlan->bindings[5].addr != ctr::native::kCdControl ||
      platformPlan->windowLo[0] != ctr::native::kGpuTimeoutArm ||
      platformPlan->windowHi[0] != ctr::native::kProjectionWindowEnd ||
      platformPlan->windowLo[1] != ctr::native::kCdSync ||
      platformPlan->windowHi[1] != ctr::native::kCdControlWindowEnd) {
    std::fprintf(stderr, "CtrRuntime lost its measured platform-HLE plan\n");
    return 1;
  }

  ctr::installRuntimeOwners(*game);
  FrameLoopShell shell;
  shell.prepareProduct(*game);
  if (game->disc.env_key == nullptr || std::string_view(game->disc.env_key) != "PSXPORT_CTR_DISC" ||
      core.rsub.mode.path() != RenderPath::Gte || game->platform_hle.lookup(ctr::native::kSetGeomScreen) == nullptr ||
      game->platform_hle.lookup(ctr::native::kSetGeomOffset) == nullptr ||
      game->platform_hle.lookup(ctr::native::kVSync) == nullptr ||
      game->platform_hle.lookup(ctr::native::kGpuTimeoutArm) == nullptr ||
      game->platform_hle.lookup(ctr::native::kGpuTimeoutCheck) == nullptr ||
      game->platform_hle.lookup(ctr::native::kCdRead) == nullptr ||
      game->platform_hle.lookup(ctr::native::kCdReadSync) == nullptr ||
      game->platform_hle.lookup(ctr::native::kCdSync) == nullptr ||
      game->platform_hle.lookup(ctr::native::kCdControl) == nullptr ||
      game->platform_hle.lookup(ctr::native::kProjectionWindowEnd) != nullptr ||
      game->platform_hle.lookup(ctr::native::kCdControlWindowEnd) != nullptr) {
    std::fprintf(stderr, "production composition allowed a successful retail VSync call\n");
    return 1;
  }

  game->timing.vblank = 11u;
  game->platform_hle.lookup(ctr::native::kGpuTimeoutArm)(&core);
  if (core.mem_r32(ctr::native::kGpuTimeoutDeadline) != 251u || core.mem_r32(ctr::native::kGpuTimeoutPollCount) != 0u ||
      core.r[2] != 251u) {
    std::fprintf(stderr, "native CTR GPU timeout arm did not use the host-owned field counter\n");
    return 1;
  }
  game->platform_hle.lookup(ctr::native::kGpuTimeoutCheck)(&core);
  if (core.mem_r32(ctr::native::kGpuTimeoutPollCount) != 1u || core.r[2] != 0u) {
    std::fprintf(stderr, "native CTR GPU timeout check did not preserve the unexpired queue contract\n");
    return 1;
  }
  GpuTimeoutCall expiredTimeout{game.get()};
  if (!operationAborts(invokeExpiredGpuTimeout, &expiredTimeout)) {
    std::fprintf(stderr, "native CTR GPU timeout check concealed a violated synchronous-GPU contract\n");
    return 1;
  }
  game->timing.vblank = 0u;

  constexpr uint32_t kFakeCdResult = 0x00050000u;
  for (uint32_t offset = 0; offset < 8u; ++offset) {
    core.mem_w8(kFakeCdResult + offset, 0xFFu);
  }
  core.r[5] = kFakeCdResult;
  game->platform_hle.lookup(ctr::native::kCdSync)(&core);
  if (core.r[2] != 2u) {
    std::fprintf(stderr, "native CTR CdSync did not report synchronous readiness\n");
    return 1;
  }
  for (uint32_t offset = 0; offset < 8u; ++offset) {
    if (core.mem_r8(kFakeCdResult + offset) != 0u) {
      std::fprintf(stderr, "native CTR CdSync did not initialize its result bytes\n");
      return 1;
    }
  }
  core.r[4] = 1u;
  core.r[5] = 0u;
  core.r[6] = kFakeCdResult;
  game->platform_hle.lookup(ctr::native::kCdControl)(&core);
  if (core.r[2] != 0u) {
    std::fprintf(stderr, "native CTR CdControl did not preserve the title's success contract\n");
    return 1;
  }
  game->cd.setloc_lba = 123;
  core.r[4] = 0u;
  core.r[5] = kFakeCdResult;
  core.r[6] = 0x80u;
  game->platform_hle.lookup(ctr::native::kCdRead)(&core);
  if (core.r[2] != 1u || game->cd.setloc_lba != 123) {
    std::fprintf(stderr, "native CTR CdRead did not preserve the shared synchronous zero-sector contract\n");
    return 1;
  }
  for (uint32_t offset = 0; offset < 8u; ++offset) {
    core.mem_w8(kFakeCdResult + offset, 0xFFu);
  }
  core.r[4] = 0u;
  core.r[5] = kFakeCdResult;
  game->platform_hle.lookup(ctr::native::kCdReadSync)(&core);
  if (core.r[2] != 0u) {
    std::fprintf(stderr, "native CTR CdReadSync did not report zero sectors remaining\n");
    return 1;
  }
  for (uint32_t offset = 0; offset < 8u; ++offset) {
    if (core.mem_r8(kFakeCdResult + offset) != 0u) {
      std::fprintf(stderr, "native CTR CdReadSync did not initialize its result bytes\n");
      return 1;
    }
  }
  for (const int32_t mode : std::array<int32_t, 5>{-1, 0, 1, 4, 30}) {
    VSyncCall call{game.get(), mode};
    if (!operationAborts(invokeVSync, &call)) {
      std::fprintf(stderr, "production composition allowed VSync(%d) to return\n", mode);
      return 1;
    }
  }

  gte_bind(&core);
  core.r[4] = 320u;
  game->platform_hle.lookup(ctr::native::kSetGeomScreen)(&core);
  core.r[4] = 256u;
  core.r[5] = 120u;
  game->platform_hle.lookup(ctr::native::kSetGeomOffset)(&core);
  if (!core.rsub.projParams.geomValid() || core.rsub.projParams.geomH() != 320.0f ||
      core.rsub.projParams.geomOfx() != 256.0f || core.rsub.projParams.geomOfy() != 120.0f) {
    std::fprintf(stderr, "projection HLE did not record the measured boot projection\n");
    return 1;
  }

  constexpr uint32_t kFakeGp = 0x00010000u;
  constexpr uint32_t kFakeGame = 0x00020000u;
  constexpr uint32_t kFakeFrame = 0x00030000u;
  core.r[28] = kFakeGp;
  core.r[20] = kFakeFrame;
  core.mem_w32(kFakeGp + 832u, kFakeGame);
  core.mem_w32(kFakeGame + 9580u, 4096u); // Exercise the retail branch which would call VSync(0).
  core.mem_w32(kFakeView + 0x18u, 512u);
  core.mem_w16(kFakeView + 0x20u, 320u);
  core.mem_w16(kFakeView + 0x22u, 240u);
  ProjectionMismatchCall mismatch{&core};
  if (!operationAborts(invokeProjectionMismatch, &mismatch)) {
    std::fprintf(stderr, "projection owner accepted a retail publication which contradicted its view input\n");
    return 1;
  }

  shell.step(core, 0);
  auto *driver = dynamic_cast<ctr::CtrFrameDriver *>(game->frameDriver.get());
  const std::array<uint32_t, 4> firstDispatches{
      ctr::native::kExecutableEntry,
      ctr::native::kAfterFirstStartupVSync,
      ctr::native::kAfterSecondStartupVSync,
      ctr::native::kFrameSuffix,
  };
  const std::array<uint32_t, 4> firstSupers{
      ctr::native::kStartupGpuInit,
      ctr::native::kStartupDisplayInit,
      ctr::native::kProjectionProducer,
      ctr::native::kFrameTiming,
  };
  if (!driver || driver->completedFrames() != 1 || g_dispatchedCore != &core || g_dispatchCount != 4 ||
      !std::equal(firstDispatches.begin(), firstDispatches.end(), g_dispatchTrace.begin()) || g_superCount != 4 ||
      !std::equal(firstSupers.begin(), firstSupers.end(), g_superTrace.begin()) ||
      g_dispatchRaTrace[1] != ctr::native::kAfterFirstStartupVSync || g_dispatchA0Trace[1] != 0u ||
      g_dispatchRaTrace[2] != ctr::native::kAfterSecondStartupVSync || g_dispatchA0Trace[2] != 0u ||
      g_dispatchRaTrace[3] != ctr::native::kFrameSuffix || g_dispatchA0Trace[3] != 0u ||
      core.mem_r32(kFakeFrame + 7388u) != 0x00C0FFEEu || game->timing.guestInstructionTicks != 13u ||
      game->timing.logicFrame != 0u || game->timing.vblank != 1u ||
      driver->projection().current().source != kFakeView || driver->projection().current().nativeWidth != 320 ||
      driver->projection().current().nativeHeight != 240 || driver->projection().current().centerX != 160 ||
      driver->projection().current().centerY != 120 || driver->projection().current().screenDistance != 512u ||
      driver->projection().current().sequence != 1u || driver->presentation().completedFences() != 1u ||
      game->presentation.fence() != 1u || core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not cross startup into the exact first frame transition\n");
    return 1;
  }
  for (ctr::CtrRuntime::RecompiledOverride function : g_overrides) {
    if (function != nullptr) {
      std::fprintf(stderr, "frame-scoped generated override leaked past the first frame\n");
      return 1;
    }
  }

  shell.step(core, 1);
  if (driver->completedFrames() != 2 || g_dispatchCount != 6 || g_dispatchTrace[4] != ctr::native::kFrameLoopResume ||
      g_dispatchTrace[5] != ctr::native::kFrameSuffix || g_superCount != 6 ||
      g_superTrace[4] != ctr::native::kProjectionProducer || g_superTrace[5] != ctr::native::kFrameTiming ||
      game->timing.guestInstructionTicks != 22u || game->timing.logicFrame != 1u || game->timing.vblank != 2u ||
      driver->projection().previous().sequence != 1u || driver->projection().current().sequence != 2u ||
      driver->presentation().completedFences() != 2u || game->presentation.fence() != 2u ||
      core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not repeat from the measured frame-loop resume\n");
    return 1;
  }
  g_routeShutdownOnResume = true;
  shell.step(core, 2);
  if (driver->completedFrames() != 3 || g_dispatchCount != 9 || g_dispatchTrace[6] != ctr::native::kFrameLoopResume ||
      g_dispatchTrace[7] != ctr::native::kAfterShutdownVSync ||
      g_dispatchRaTrace[7] != ctr::native::kAfterShutdownVSync || g_dispatchA0Trace[7] != 30u ||
      g_dispatchTrace[8] != ctr::native::kFrameSuffix || g_superCount != 9 ||
      g_superTrace[6] != ctr::native::kShutdownDisplay || g_superTrace[7] != ctr::native::kProjectionProducer ||
      g_superTrace[8] != ctr::native::kFrameTiming || game->timing.guestInstructionTicks != 33u ||
      game->timing.logicFrame != 2u || game->timing.vblank != 3u || driver->presentation().completedFences() != 3u ||
      game->presentation.fence() != 3u || core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not preserve the measured shutdown VSync transition\n");
    return 1;
  }

  constexpr uint32_t kPumpStack = 0x00070000u;
  core.r[29] = kPumpStack;
  g_routeBootResourcePumpOnResume = true;
  shell.step(core, 3);
  if (driver->completedFrames() != 4 || g_dispatchCount != 12 || g_dispatchTrace[9] != ctr::native::kFrameLoopResume ||
      g_dispatchTrace[10] != ctr::native::kBootResourcePumpBegin ||
      g_dispatchTrace[11] != ctr::native::kBootResourcePumpPoll || g_bootResourcePolls != 1u ||
      g_bootResourceCommitPolls != 0u || core.r[29] != kPumpStack - 32u || game->timing.guestInstructionTicks != 43u ||
      game->timing.vblank != 4u || driver->presentation().completedFences() != 4u || game->presentation.fence() != 4u ||
      core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not yield the first async resource poll to the host\n");
    return 1;
  }

  shell.step(core, 4);
  if (driver->completedFrames() != 5 || g_dispatchCount != 15 ||
      g_dispatchTrace[12] != ctr::native::kBootResourcePumpPoll ||
      g_dispatchTrace[13] != ctr::native::kBootResourcePumpCommit ||
      g_dispatchTrace[14] != ctr::native::kBootResourcePumpCommitPoll || g_bootResourcePolls != 2u ||
      g_bootResourceCommitPolls != 1u || core.r[29] != kPumpStack - 32u || game->timing.guestInstructionTicks != 53u ||
      game->timing.vblank != 5u || driver->presentation().completedFences() != 5u || game->presentation.fence() != 5u ||
      core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not advance into the finite resource-commit poll\n");
    return 1;
  }

  shell.step(core, 5);
  if (driver->completedFrames() != 6 || g_dispatchCount != 18 ||
      g_dispatchTrace[15] != ctr::native::kBootResourcePumpCommitPoll ||
      g_dispatchTrace[16] != ctr::native::kBootResourcePumpReturn ||
      g_dispatchRaTrace[16] != ctr::native::kBootResourcePumpReturn ||
      g_dispatchTrace[17] != ctr::native::kFrameSuffix || g_bootResourceCommitPolls != 2u || core.r[29] != kPumpStack ||
      game->timing.guestInstructionTicks != 70u || game->timing.logicFrame != 5u || game->timing.vblank != 6u ||
      driver->presentation().completedFences() != 6u || game->presentation.fence() != 6u ||
      core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not restore the resource-pump frame and exact caller continuation\n");
    return 1;
  }
  for (ctr::CtrRuntime::RecompiledOverride function : g_overrides) {
    if (function != nullptr) {
      std::fprintf(stderr, "frame-scoped generated override leaked past the resource-pump continuation\n");
      return 1;
    }
  }
  g_routeStartupAudioWaitOnResume = true;
  shell.step(core, 6);
  if (driver->completedFrames() != 7 || g_dispatchTrace[g_dispatchCount - 1u] != ctr::native::kFrameLoopResume ||
      g_startupAudioServiceCalls != 1u || core.mem_r32(ctr::native::kStartupAudioWaitState) != 1u ||
      driver->presentation().completedFences() != 7u || game->presentation.fence() != 7u) {
    std::fprintf(stderr, "production driver did not yield the startup XA wait after one preserved service call\n");
    return 1;
  }
  shell.step(core, 7);
  if (driver->completedFrames() != 8 ||
      g_dispatchTrace[g_dispatchCount - 2u] != ctr::native::kStartupAudioServiceReturn ||
      g_startupAudioServiceCalls != 2u || core.mem_r32(ctr::native::kStartupAudioWaitState) != 0u ||
      driver->presentation().completedFences() != 8u || game->presentation.fence() != 8u) {
    std::fprintf(stderr, "production driver did not resume the retail startup XA poll after host audio progress\n");
    return 1;
  }
  constexpr uint32_t kRaceWaitStack = 0x00080000u;
  constexpr uint32_t kRaceWaitCompletion = 0x00081000u;
  core.r[29] = kRaceWaitStack;
  core.r[4] = 0x00082000u;
  core.r[5] = 0u;
  core.r[6] = 0u;
  core.r[7] = kRaceWaitCompletion;
  core.mem_w32(kRaceWaitStack + 16u, 0xFFFFFFFFu);
  core.mem_w32(kRaceWaitCompletion, 0u);
  const std::size_t raceDispatchBegin = g_dispatchCount;
  const std::size_t raceSuperBegin = g_superCount;
  g_routeBootResourceRaceWaitOnResume = true;
  shell.step(core, 8);
  if (driver->completedFrames() != 9u || core.r[29] != kRaceWaitStack - 72u ||
      g_dispatchCount != raceDispatchBegin + 4u ||
      g_dispatchTrace[raceDispatchBegin] != ctr::native::kFrameLoopResume ||
      g_dispatchTrace[raceDispatchBegin + 1u] != 0x8003E978u ||
      g_dispatchTrace[raceDispatchBegin + 2u] != 0x800321B4u ||
      g_dispatchTrace[raceDispatchBegin + 3u] != 0x80031EE4u || driver->presentation().completedFences() != 9u ||
      game->presentation.fence() != 9u) {
    std::fprintf(stderr, "production driver did not begin the race/menu resource wait on a native field\n");
    return 1;
  }
  shell.step(core, 9);
  if (driver->completedFrames() != 10u || core.r[29] != kRaceWaitStack - 72u ||
      g_dispatchCount != raceDispatchBegin + 4u || driver->presentation().completedFences() != 10u ||
      game->presentation.fence() != 10u) {
    std::fprintf(stderr, "production driver did not preserve the race/menu wait across its second native field\n");
    return 1;
  }
  shell.step(core, 10);
  if (driver->completedFrames() != 11u || core.r[29] != kRaceWaitStack || g_dispatchCount != raceDispatchBegin + 8u ||
      g_dispatchTrace[raceDispatchBegin + 4u] != ctr::native::kBootResourceWaitReturn ||
      g_dispatchTrace[raceDispatchBegin + 5u] != ctr::native::kBootResourceWaitRaceCaller ||
      g_dispatchTrace[raceDispatchBegin + 6u] != ctr::native::kBootResourceWaitRaceResume ||
      g_dispatchTrace[raceDispatchBegin + 7u] != ctr::native::kFrameSuffix || g_superCount != raceSuperBegin + 3u ||
      g_superTrace[raceSuperBegin] != ctr::native::kFrameTiming ||
      g_superTrace[raceSuperBegin + 1u] != ctr::native::kProjectionProducer ||
      g_superTrace[raceSuperBegin + 2u] != ctr::native::kFrameTiming ||
      driver->presentation().completedFences() != 11u || game->presentation.fence() != 11u ||
      core.idiag.otattr_depth != 0) {
    std::fprintf(stderr, "production driver did not restore 0x80033610 and resume its sole direct caller\n");
    return 1;
  }
  if (driver->presentation().hasPresentableCapture(core) || driver->presentation().presentedFences() != 0u) {
    std::fprintf(stderr, "presentation owner treated an empty field as a renderable capture\n");
    return 1;
  }
  const RqItem capturedRetailItem{};
  game->presentation.capture(&capturedRetailItem, 1);
  if (!driver->presentation().hasPresentableCapture(core)) {
    std::fprintf(stderr, "presentation owner would discard a captured retail render item\n");
    return 1;
  }
  ctr::DiscReadOwner discRead;
  core.mem_w32(ctr::native::kCdReadCompletionCallback, 0u);
  const std::size_t dispatchesBeforeRead = g_dispatchCount;
  discRead.deliverCompletion(core, runtime);
  core.mem_w32(ctr::native::kCdReadCompletionCallback, kRetailReadCompletionCallback);
  core.r[2] = 0x0BADC0DEu;
  core.r[31] = 0x0F1E2D3Cu;
  discRead.deliverCompletion(core, runtime);
  if (g_dispatchCount != dispatchesBeforeRead + 1u ||
      g_dispatchTrace[g_dispatchCount - 1u] != kRetailReadCompletionCallback ||
      g_dispatchA0Trace[g_dispatchCount - 1u] != ctr::native::kCdlComplete ||
      core.mem_r32(ctr::native::kCdReadCompletionCallback) != 0u || core.r[2] != 0x0BADC0DEu ||
      core.r[31] != 0x0F1E2D3Cu || discRead.deliveredCallbacks() != 1u || discRead.polledReads() != 1u) {
    std::fprintf(stderr, "CTR disc-read owner did not deliver the retail libcd read-completion callback\n");
    return 1;
  }
  ctr::CtrRuntime dmaRuntime(captureDmaDispatch, ctr::native::kExecutableEntry);
  ctr::DmaCallbackOwner dmaOwner({.owed = fakeDmaOwed, .take = fakeDmaTake, .ack = fakeDmaAck});
  constexpr uint32_t kRetailSpuDmaWrapper = 0x8007AB34u;
  core.mem_w32(ctr::native::kSpuDmaCallbackSlot, kRetailSpuDmaWrapper);
  core.r[2] = 0x12345678u;
  core.r[31] = 0x87654321u;
  g_dmaCompletionOwed = true;
  if (!dmaOwner.serviceSpu(core, dmaRuntime) || g_dmaCompletionOwed ||
      g_dmaTakenChannel != ctr::native::kSpuDmaChannel || g_dmaAckedChannel != ctr::native::kSpuDmaChannel ||
      g_dmaDispatchAddress != kRetailSpuDmaWrapper || !g_dmaDispatchInIrq || core.game->hle.in_irq != 0 ||
      core.r[2] != 0x12345678u || core.r[31] != 0x87654321u || dmaOwner.serviceSpu(core, dmaRuntime)) {
    std::fprintf(stderr, "native CTR DMA owner did not deliver exactly one owed retail SPU callback\n");
    return 1;
  }
  core.game->hle.in_irq = 1;
  g_dmaCompletionOwed = true;
  if (dmaOwner.serviceSpu(core, dmaRuntime) || !g_dmaCompletionOwed) {
    std::fprintf(stderr, "native CTR DMA owner consumed a nested completion before IRQ return\n");
    return 1;
  }
  core.game->hle.in_irq = 0;
  g_chainDmaDuringDispatch = true;
  if (!dmaOwner.serviceSpu(core, dmaRuntime) || !dmaOwner.hasPendingSpu() || !g_dmaCompletionOwed ||
      core.game->hle.in_irq != 0) {
    std::fprintf(stderr, "native CTR DMA owner lost a synchronously chained completion\n");
    return 1;
  }
  g_chainDmaDuringDispatch = false;
  if (!dmaOwner.serviceSpu(core, dmaRuntime) || dmaOwner.hasPendingSpu() || g_dmaCompletionOwed) {
    std::fprintf(stderr, "native CTR DMA owner did not defer the chained completion to a later safe boundary\n");
    return 1;
  }

  ctr::CtrRuntime callbackRuntime(
      captureFrameCallbackDispatch, ctr::native::kExecutableEntry, captureOverride, captureSuper);
  ctr::FrameCallbackOwner frameCallbacks;
  core.r[4] = 0x80034AA4u;
  const std::size_t callbackSuperBegin = g_superCount;
  frameCallbacks.observeVblankRegistration(core, callbackRuntime);
  core.mem_w32(ctr::native::kDrawSyncCallbackSlot, 0x80034A80u);
  core.mem_w8(kFakeGame + ctr::native::kDrawSyncPendingOffset, 1u);
  core.mem_w32(kFakeGame + ctr::native::kFrameCallbackCountOffset, 3u);
  core.mem_w32(kFakeGp + ctr::native::kFrameWaitFieldsGpOffset, 2u);
  core.r[2] = 0x12345678u;
  core.r[31] = 0x87654321u;
  frameCallbacks.deliverField(core, callbackRuntime);
  if (frameCallbacks.vblankCallback() != 0x80034AA4u || g_frameCallbackCount != 2u ||
      g_frameCallbackTrace[0] != 0x80034A80u || g_frameCallbackTrace[1] != 0x80034AA4u || !g_frameCallbackInIrq ||
      core.game->hle.in_irq != 0 || core.mem_r8(kFakeGame + ctr::native::kDrawSyncPendingOffset) != 0u ||
      core.mem_r32(kFakeGame + ctr::native::kFrameCallbackCountOffset) != 4u ||
      core.mem_r32(kFakeGp + ctr::native::kFrameWaitFieldsGpOffset) != 1u || core.r[2] != 0x12345678u ||
      core.r[31] != 0x87654321u || g_superCount != callbackSuperBegin + 1u ||
      g_superTrace[callbackSuperBegin] != ctr::native::kVblankCallbackInstall) {
    std::fprintf(stderr, "native CTR field owner did not deliver the registered draw/VBlank callbacks in order\n");
    return 1;
  }
  core.game->hle.in_irq = 1;
  core.mem_w8(kFakeGame + ctr::native::kDrawSyncPendingOffset, 1u);
  frameCallbacks.deliverField(core, callbackRuntime);
  if (g_frameCallbackCount != 2u || core.mem_r8(kFakeGame + ctr::native::kDrawSyncPendingOffset) != 1u) {
    std::fprintf(stderr, "native CTR field owner delivered callbacks recursively inside an IRQ\n");
    return 1;
  }
  core.game->hle.in_irq = 0;
  if (game_guest_vram_is_picture(*game)) {
    std::fprintf(stderr, "CtrRuntime claimed picture ownership without a presented frame\n");
    return 1;
  }

  std::puts("CtrRuntime: GTE-only player capability, A/B projection publication, unpresented repeating fences, "
            "and fatal VSync ownership");
  return 0;
}
