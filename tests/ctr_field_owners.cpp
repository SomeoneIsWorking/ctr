// FieldBoundary, FieldOverrideScope and the other owners split out of CtrFrameDriver.

#include "core.h"
#include "ctr_runtime.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "field_boundary.h"
#include "field_override_scope.h"
#include "frame_suffix.h"
#include "game.h"
#include "game_runtime.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "native_ownership.h"
#include "scene_cut.h"
#include "spu_dma_callback_registration.h"
#include "startup_audio_wait.h"
#include "startup_resource_load.h"
#include "startup_resource_pump.h"
#include "vsync_bridge.h"

#include <array>
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

constexpr std::uint32_t kReturn = 0x03e00008u;     // jr ra
constexpr std::uint32_t kSetV0One = 0x24020001u;   // addiu v0, zero, 1
constexpr std::uint32_t kClearV0 = 0x24020000u;    // addiu v0, zero, 0
constexpr std::uint32_t kMarkS0Five = 0x24100005u; // addiu s0, zero, 5

void installReturn(Core &core, std::uint32_t address) {
  core.mem_w32(address, kReturn);
  core.mem_w32(address + 4u, 0u);
  core.mem_w32(address + 8u, 0u);
}

void installReturnSetting(Core &core, std::uint32_t address, std::uint32_t setting) {
  core.mem_w32(address, setting);
  core.mem_w32(address + 4u, kReturn);
  core.mem_w32(address + 8u, 0u);
}

void installReturnTo(Core &core, std::uint32_t address, std::uint32_t boundary) {
  core.mem_w32(address, 0x3C1F8003u | ((boundary >> 16u) & 0xFFFFu));
  core.mem_w32(address + 4u, 0x37FF0000u | (boundary & 0xFFFFu));
  core.mem_w32(address + 8u, kReturn);
  core.mem_w32(address + 12u, 0u);
}

constexpr std::uint32_t jump(std::uint32_t address) {
  return 0x08000000u | ((address >> 2u) & 0x03ffffffu);
}

std::uint32_t continuationsReached = 0;

void endTheField(Core *core) {
  ++continuationsReached;
  psx::cpu::requestExecutionExit(*core, psx::cpu::ExecutionExitReason::FrameBoundary);
}

bool pendingIsFrameBoundary(Core &core) {
  const auto &pending = core.executionControl().pending();
  return pending && pending->reason == psx::cpu::ExecutionExitReason::FrameBoundary;
}

void clearPending(Core &core) {
  (void)core.executionControl().consume();
}

constexpr std::uint32_t kStack = 0x00120000u;
constexpr std::uint32_t kGlobalBlock = 0x00130000u;
constexpr std::uint32_t kCallerFifthArgumentSlot = 16u;
constexpr std::uint32_t kLoadFrameBytes = 72u;
constexpr std::uint32_t kLoadSavedReturnSlot = 68u;

} // namespace

int main() {
  constexpr std::uint32_t kEntry = 0x80010000u;
  ctr::CtrRuntime runtime(kEntry);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.imageCatalog().activate("ctr-field-owners", {0x00010000u, 0x0008d800u}, 1u);
  core.r[28] = kGlobalBlock;
  core.r[29] = kStack;

  ctr::SceneCut sceneCut;
  ctr::PresentationOwner presentation(sceneCut);
  ctr::FieldBoundary field(presentation);

  check(!field.pending(core), "a fresh field boundary reported a pending field");
  check(field.completedFields() == 0u, "a fresh field boundary counted a field");
  psx::cpu::requestExecutionExit(core, psx::cpu::ExecutionExitReason::FrameBoundary);
  check(field.pending(core), "a typed FrameBoundary exit raised by the framework was not read as a finished field");
  clearPending(core);
  check(!field.pending(core), "the boundary stayed pending after its typed exit was consumed");
  field.request(core);
  check(field.pending(core), "the title's own request was not read as a finished field");
  check(pendingIsFrameBoundary(core), "requesting a boundary did not raise a typed FrameBoundary exit");
  field.finishField(core, 0u);
  check(field.completedFields() == 1u, "finishing a field did not count exactly one field");
  check(presentation.completedFences() == 1u, "finishing a field did not advance exactly one fence");
  check(!core.executionControl().pending(), "finishing a field left its typed exit behind");
  field.beginField();
  check(!field.pending(core), "beginning a field did not clear the previous field's request");

  constexpr std::uint32_t kTracker = 0x80096000u;
  core.mem_w32(kGlobalBlock + ctr::native::kGameStateGpOffset, kTracker);
  core.mem_w32(kGlobalBlock + ctr::native::kMainStateGpOffset, 3u);
  core.mem_w32(kTracker + ctr::native::kTrackerLevelIdOffset, 0x29u);
  presentation.finishField(core);
  check(sceneCut.isCut(), "a new scene identity was not a cut");
  presentation.finishField(core);
  check(!sceneCut.isCut(), "an unchanged scene was declared a cut");
  core.mem_w32(kTracker + ctr::native::kTrackerGameModeOffset, ctr::native::kGameModeLoading);
  presentation.finishField(core);
  check(sceneCut.isCut(), "the loading bit rising was not a cut");
  core.mem_w32(kTracker + ctr::native::kTrackerGameModeOffset, 0u);
  core.mem_w32(kTracker + ctr::native::kTrackerLevelIdOffset, 0u);
  presentation.finishField(core);
  check(sceneCut.isCut(), "a level id change was not a cut");
  presentation.finishField(core);
  check(!sceneCut.isCut(), "a settled scene stayed a cut");

  const std::array<ctr::FieldOverrideBinding, 2> bindings{{
      {ctr::native::kFrameTiming, "field-owners test owner", endTheField},
      {ctr::native::kProjectionProducer, "field-owners second owner", endTheField},
  }};
  const auto image = core.currentImageIdentity(ctr::native::kFrameTiming);
  const auto secondImage = core.currentImageIdentity(ctr::native::kProjectionProducer);
  check(image.has_value() && secondImage.has_value(), "the synthetic image does not own the field override addresses");
  if (image && secondImage) {
    {
      const ctr::FieldOverrideScope scope(core, bindings);
      check(scope.installedCount() == 2u, "a field did not install every row");
      check(core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}),
            "the field's own override was not installed");
      check(core.nativeDispatcher().isInstalled({*secondImage, ctr::native::kProjectionProducer}),
            "the field's second override was not installed");
    }
    check(!core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}) &&
              !core.nativeDispatcher().isInstalled({*secondImage, ctr::native::kProjectionProducer}),
          "a field's overrides outlived their scope");
  }

  check(!ctr::frameSuffixStillWaiting({}), "an absent game-state descriptor was read as a pending draw");
  check(!ctr::frameSuffixStillWaiting({.drawSyncPending = true, .vblankFieldsPending = true}),
        "outstanding work was honoured without a game-state descriptor");
  check(ctr::frameSuffixStillWaiting({.gameState = kGlobalBlock, .drawSyncPending = true}),
        "a pending DrawSync byte did not keep the suffix waiting");
  check(ctr::frameSuffixStillWaiting({.gameState = kGlobalBlock, .vblankFieldsPending = true}),
        "a pending VSync field countdown did not keep the suffix waiting");
  check(!ctr::frameSuffixStillWaiting({.gameState = kGlobalBlock}),
        "a published draw with nothing outstanding still waited");
  check(!ctr::frameSuffixStillWaiting({.gameState = kGlobalBlock, .drawSyncPending = true, .registeredCallbacks = 7u}),
        "the suffix kept waiting past its registered-callback limit");
  check(
      ctr::frameSuffixStillWaiting({.gameState = kGlobalBlock, .vblankFieldsPending = true, .registeredCallbacks = 6u}),
      "the suffix stopped waiting one callback short of its limit");

  installReturnSetting(core, ctr::native::kStartupGpuInit, kMarkS0Five);
  check(
      psx::cpu::tryInstallNativeOverride(core, ctr::native::kAfterFirstStartupVSync, "vsync continuation", endTheField)
          .has_value(),
      "could not install the VSync continuation owner");
  core.r[16] = 0u;
  core.r[31] = ctr::native::kStartupGpuInitReturn;
  continuationsReached = 0;
  const ctr::VsyncBridge bridge{
      ctr::native::kStartupGpuInit, ctr::native::kStartupGpuInitReturn, ctr::native::kAfterFirstStartupVSync, 0u};
  ctr::runVsyncBridge(core, runtime, bridge);
  check(core.r[16] == 5u, "the VSync bridge did not preserve the retail callee's own body");
  check(core.r[31] == ctr::native::kAfterFirstStartupVSync,
        "the VSync bridge did not restore the measured continuation into ra");
  check(core.r[4] == 0u, "the VSync bridge did not carry the measured mode argument across");
  check(continuationsReached == 1u, "the VSync bridge did not resume exactly once at its continuation");
  check(pendingIsFrameBoundary(core), "the VSync bridge's continuation did not end the field");
  clearPending(core);
  check(psx::cpu::removeNativeOverride(core, ctr::native::kAfterFirstStartupVSync), "could not remove the VSync owner");

  ctr::StartupResourceLoad load(runtime, field);
  installReturnSetting(core, ctr::native::kBootResourceWait, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourcePreWaitHelper, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourceSetup, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourceCommit, kSetV0One);
  installReturnTo(core, ctr::native::kBootResourceWaitReturn, ctr::native::kBootResourceWaitFirstCaller);
  check(psx::cpu::tryInstallNativeOverride(
            core, ctr::native::kBootResourceWaitFirstCaller, "load continuation", endTheField)
            .has_value(),
        "could not install the resource-load continuation owner");

  core.r[29] = kStack;
  core.r[6] = 0u; // selects the pre-wait helper, which must be reached
  core.mem_w32(kStack + kCallerFifthArgumentSlot, 0u);
  load.begin(core);
  check(!load.ownsSuffix(), "a load with a non-(-1) fifth argument claimed a suffix");
  check(core.r[2] == 1u, "a load with a non-(-1) fifth argument did not run retail's own body");
  check(core.r[29] == kStack, "a load with a non-(-1) fifth argument created a frame anyway");
  check(!field.pending(core), "a load with a non-(-1) fifth argument ended the field anyway");

  core.r[29] = kStack;
  core.r[6] = 0u;
  core.r[31] = ctr::native::kBootResourceWaitFirstCaller;
  core.mem_w32(kStack + kCallerFifthArgumentSlot, 0xFFFFFFFFu);
  load.begin(core);
  check(load.ownsSuffix(), "the -1 branch did not claim the suffix it owes");
  check(pendingIsFrameBoundary(core), "the -1 branch did not end the field the omitted VSync owed");
  check(core.r[4] == 2u, "the omitted VSync did not leave a0 carrying its measured mode");
  check(core.r[29] == kStack - kLoadFrameBytes, "the -1 branch did not create the frame it writes into");
  check(core.mem_r32(kStack - kLoadFrameBytes + kLoadSavedReturnSlot) == ctr::native::kBootResourceWaitFirstCaller,
        "the -1 branch did not spill the caller's return into the frame it created");
  check(load.consumeWaitedField(), "the -1 branch did not owe the field it counted");
  check(!load.consumeWaitedField(), "the -1 branch owed more fields than it counted");
  check(!load.consumeWaitedField(), "the owed-field countdown ran below zero");
  check(field.pending(core), "consuming the owed field stopped the field being finished");
  clearPending(core);

  continuationsReached = 0;
  load.resume(core);
  check(continuationsReached == 1u, "the resource-load suffix did not resume the measured caller");
  check(pendingIsFrameBoundary(core), "the resource-load suffix did not end the field");
  check(!load.ownsSuffix(), "the resource-load suffix kept its continuation after running it");
  clearPending(core);
  check(psx::cpu::removeNativeOverride(core, ctr::native::kBootResourceWaitFirstCaller),
        "could not remove the resource-load continuation owner");

  ctr::StartupResourcePump pump(runtime, field);
  field.beginField();
  check(!pump.isActive() && pump.phase() == ctr::StartupResourcePump::Phase::Inactive,
        "a fresh resource pump claimed a phase");
  check(psx::cpu::tryInstallNativeOverride(core, ctr::native::kBootResourcePumpReturn, "pump continuation", endTheField)
            .has_value(),
        "could not install the resource-pump continuation owner");
  installReturnSetting(core, ctr::native::kBootResourcePumpBegin, kClearV0);
  installReturnSetting(core, ctr::native::kBootResourcePumpCommit, kClearV0);

  installReturnSetting(core, ctr::native::kBootResourcePumpPoll, kClearV0); // the stage is not finished
  core.r[29] = kStack;
  core.r[31] = ctr::native::kBootResourcePumpReturn;
  pump.begin(core);
  check(pump.isActive() && pump.phase() == ctr::StartupResourcePump::Phase::ResourcePoll,
        "a false resource poll did not keep the pump in its first phase");
  check(pendingIsFrameBoundary(core), "a false resource poll did not yield the field");
  check(core.r[29] == kStack - 32u, "the resource pump did not create the frame it writes into");
  clearPending(core);

  installReturnSetting(core, ctr::native::kBootResourcePumpPoll, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourcePumpCommitPoll, kClearV0); // the commit is not finished
  continuationsReached = 0;
  pump.resume(core);
  check(pump.isActive() && pump.phase() == ctr::StartupResourcePump::Phase::CommitPoll,
        "a false commit poll did not keep the pump in its second phase");
  check(continuationsReached == 0u, "a false commit poll ran the continuation anyway");
  check(pendingIsFrameBoundary(core), "a false commit poll did not yield the field");
  clearPending(core);

  installReturnSetting(core, ctr::native::kBootResourcePumpCommitPoll, kSetV0One);
  pump.resume(core);
  check(!pump.isActive() && pump.phase() == ctr::StartupResourcePump::Phase::Inactive,
        "the resource pump kept its phase after both polls reported progress");
  check(continuationsReached == 1u, "the finished resource pump did not resume its continuation exactly once");
  check(pendingIsFrameBoundary(core), "the finished resource pump did not end the field");
  clearPending(core);
  check(psx::cpu::removeNativeOverride(core, ctr::native::kBootResourcePumpReturn),
        "could not remove the resource-pump continuation owner");

  ctr::StartupAudioWait audio(runtime, field);
  field.beginField();
  installReturnSetting(core, ctr::native::kStartupAudioService, kSetV0One);
  check(psx::cpu::tryInstallNativeOverride(core, ctr::native::kStartupAudioLoop, "audio continuation", endTheField)
            .has_value(),
        "could not install the startup-audio continuation owner");
  core.mem_w32(ctr::native::kStartupAudioLoop, jump(ctr::native::kFrameTiming));
  core.mem_w32(ctr::native::kStartupAudioLoop + 4u, 0u);
  check(psx::cpu::tryInstallNativeOverride(core, ctr::native::kFrameTiming, "timing boundary", endTheField).has_value(),
        "could not install the frame-timing boundary owner");
  core.mem_w32(ctr::native::kStartupAudioWaitState, 1u);

  core.r[31] = ctr::native::kBootResourceWaitReturn; // not the loop's own service callsite
  audio.service(core);
  check(!audio.ownsContinuation(), "a service call from an unmeasured caller claimed the wait");
  check(!field.pending(core), "a service call from an unmeasured caller ended the field");

  core.r[31] = ctr::native::kStartupAudioServiceReturn;
  core.mem_w32(ctr::native::kStartupAudioWaitState, 0u);
  audio.service(core);
  check(!audio.ownsContinuation(), "a completed XA task still claimed the wait");
  check(!field.pending(core), "a completed XA task still ended the field");

  core.mem_w32(ctr::native::kStartupAudioWaitState, 1u);
  audio.service(core);
  check(audio.ownsContinuation(), "a running XA task did not claim the wait");
  check(pendingIsFrameBoundary(core), "a running XA task did not end the field the wait owed");
  clearPending(core);

  continuationsReached = 0;
  audio.resume(core);
  check(continuationsReached == 1u, "the startup audio wait did not resume its continuation exactly once");
  check(!audio.ownsContinuation(), "the startup audio wait kept its continuation after resuming it");
  check(pendingIsFrameBoundary(core), "the startup audio wait did not end the field");
  clearPending(core);

  check(psx::cpu::removeNativeOverride(core, ctr::native::kFrameTiming), "could not remove the timing owner");
  check(psx::cpu::removeNativeOverride(core, ctr::native::kStartupAudioLoop), "could not remove the audio owner");

  // The retail DMACallback stores the callback in its guest table; the registration publishes it for the shared IRQ
  // path.
  constexpr std::uint32_t kSpuCallback = 0x80012340u;
  constexpr std::uint32_t kDmaCallbackProgram[] = {
      0x3C088001u, // lui t0, 0x8001
      0x35082340u, // ori t0, t0, 0x2340
      0x3C098009u, // lui t1, 0x8009
      0xAD28CB18u, // sw t0, -0x34E8(t1): the channel-4 slot at 0x8008CB18
      kReturn,
      0u,
  };
  for (std::size_t word = 0; word < std::size(kDmaCallbackProgram); ++word) {
    core.mem_w32(ctr::native::kDmaCallback + 4u * static_cast<std::uint32_t>(word), kDmaCallbackProgram[word]);
  }
  const ctr::SpuDmaCallbackRegistration registration(runtime);
  check(core.game->dmaCallbacks.current(DmaChannel::Spu) == 0u, "the SPU channel had a callback before registration");
  registration.service(core);
  check(core.game->dmaCallbacks.current(DmaChannel::Spu) == kSpuCallback,
        "registering the SPU callback did not publish it to the shared registry");
  check(core.game->dmaCallbacks.current(DmaChannel::Gpu) == 0u,
        "registering the SPU callback published another channel");

  std::printf("CTR field owners: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
