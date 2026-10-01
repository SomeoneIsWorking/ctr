// Focused coverage for the owners extracted out of `CtrFrameDriver`.
//
// The driver used to hold all of this in one 666-line body. Each owner below is constructed
// directly here rather than reached through `stepFrame`, so a test can reach the DECISION each owner
// exists to make — which boundary is pending, which branch of the startup resource load runs, where
// the resource pump had got to, whether the audio wait is owed, and what the frame suffix waits for —
// rather than only the one end-to-end path the existing frame tests drive.
//
// Every assertion here runs against the shipping owner. Nothing in this file restates a rule the
// owner implements: the guest programs below are synthetic instruction words, and the guest FIELDS
// the owners read use the `native_ownership.h` constants the owners themselves read.

#include "core.h"
#include "ctr_runtime.h"
#include "dma_callback_owner.h"
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

// Synthetic guest instruction words. CTR's resource polls read v0 as "is the stage finished", so a
// returning body that sets v0 and one that leaves it clear are the whole poll contract.
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

// A guest body for a call the executor runs with an explicit return boundary: it must put that
// boundary in ra itself, which is the whole reason the title names each continuation independently.
void installReturnTo(Core &core, std::uint32_t address, std::uint32_t boundary) {
  core.mem_w32(address, 0x3C1F8003u | ((boundary >> 16u) & 0xFFFFu));
  core.mem_w32(address + 4u, 0x37FF0000u | (boundary & 0xFFFFu));
  core.mem_w32(address + 8u, kReturn);
  core.mem_w32(address + 12u, 0u);
}

constexpr std::uint32_t jump(std::uint32_t address) {
  return 0x08000000u | ((address >> 2u) & 0x03ffffffu);
}

// A native owner that ends whatever field reaches it, which is what every one of these retail
// continuations does once its own work is done.
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

// A DMA backend that owes nothing, so the resource pump and the audio wait can be driven without
// the SPU. `owed` is the only operation either owner consults on the paths under test.
bool oweNothing(const Core &, int) {
  return false;
}
void takeNothing(Core &, int) {}
void ackNothing(Core &, int) {}

ctr::DmaCompletionBackend inertDmaBackend() {
  return {
      .owed = oweNothing,
      .take = takeNothing,
      .ack = ackNothing,
  };
}

// The field's own guest fixtures, shared by the boot owners below.
constexpr std::uint32_t kStack = 0x00120000u;
constexpr std::uint32_t kGlobalBlock = 0x00130000u;
// The caller's fifth argument, in the CALLER's frame, is the word that selects the load's branch.
constexpr std::uint32_t kCallerFifthArgumentSlot = 16u;
// The frame the -1 branch creates for itself, and the saved return word at its bottom.
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

  ctr::RenderListBoundaryDiagnostic renderListDiagnostic(false);
  ctr::PresentationOwner presentation;
  ctr::FieldBoundary field(renderListDiagnostic, presentation);
  ctr::DmaCallbackOwner dmaCallbacks(inertDmaBackend());

  // ---------------------------------------------------------------- FieldBoundary
  // Its decision: a field is finished by the title's own request OR by a typed executor exit. The
  // second route is the one a native callback uses, and the framework raises it, not this owner.
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

  // --------------------------------------------------------- FieldOverrideScope
  // Its decision: which rows belong to this field. A row says whether it is debug-only, so a
  // non-observing field drops it by that row's own claim rather than by where the row happens to sit,
  // and the field's real ownership is unaffected either way.
  const std::array<ctr::FieldOverrideBinding, 2> bindings{{
      {ctr::native::kFrameTiming, "field-owners test owner", endTheField, false},
      {ctr::native::kRenderListPublisher, "field-owners diagnostic owner", endTheField, true},
  }};
  const auto image = core.currentImageIdentity(ctr::native::kFrameTiming);
  const auto diagnosticImage = core.currentImageIdentity(ctr::native::kRenderListPublisher);
  check(image.has_value() && diagnosticImage.has_value(),
        "the synthetic image does not own the field override addresses");
  if (image && diagnosticImage) {
    {
      const ctr::FieldOverrideScope observing(runtime, core, bindings, true);
      check(observing.installedCount() == 2u, "an observing field did not install every row");
      check(core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}),
            "the field's own override was not installed");
      check(core.nativeDispatcher().isInstalled({*diagnosticImage, ctr::native::kRenderListPublisher}),
            "an observing field did not install the debug-only row");
    }
    check(!core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}) &&
              !core.nativeDispatcher().isInstalled({*diagnosticImage, ctr::native::kRenderListPublisher}),
          "an observing field's overrides outlived their scope");

    {
      const ctr::FieldOverrideScope plain(runtime, core, bindings, false);
      check(plain.installedCount() == 1u, "a non-observing field did not drop exactly the debug-only row");
      check(core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}),
            "a non-observing field dropped the field's own override instead of the debug-only row");
      check(!core.nativeDispatcher().isInstalled({*diagnosticImage, ctr::native::kRenderListPublisher}),
            "a non-observing field installed the debug-only row");
    }
    check(!core.nativeDispatcher().isInstalled({*image, ctr::native::kFrameTiming}),
          "a non-observing field's override outlived its scope");
  }

  // ------------------------------------------------------------ frameSuffixStillWaiting
  // Its decision, as a pure predicate over the three guest words. The threshold case is included
  // because that comparison is the one an off-by-one would hide.
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

  // ---------------------------------------------------------------- VsyncBridge
  // Its decision: the retail callee runs to its own return, and execution resumes at the
  // continuation with ra and a0 exactly as the omitted call would have left them.
  installReturnSetting(core, ctr::native::kStartupGpuInit, kMarkS0Five);
  check(runtime.installOverride(core, ctr::native::kAfterFirstStartupVSync, "vsync continuation", endTheField),
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
  check(runtime.removeOverride(core, ctr::native::kAfterFirstStartupVSync), "could not remove the VSync owner");

  // ------------------------------------------------------------ StartupResourceLoad
  // Its decision: the caller's fifth argument chooses the branch, and only the -1 branch is owned.
  ctr::StartupResourceLoad load(runtime, field);
  installReturnSetting(core, ctr::native::kBootResourceWait, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourcePreWaitHelper, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourceSetup, kSetV0One);
  installReturnSetting(core, ctr::native::kBootResourceCommit, kSetV0One);
  installReturnTo(core, ctr::native::kBootResourceWaitReturn, ctr::native::kBootResourceWaitFirstCaller);
  check(runtime.installOverride(core, ctr::native::kBootResourceWaitFirstCaller, "load continuation", endTheField),
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

  // The suffix runs, restores the measured caller, and hands the field back to the guest.
  continuationsReached = 0;
  load.resume(core);
  check(continuationsReached == 1u, "the resource-load suffix did not resume the measured caller");
  check(pendingIsFrameBoundary(core), "the resource-load suffix did not end the field");
  check(!load.ownsSuffix(), "the resource-load suffix kept its continuation after running it");
  clearPending(core);
  check(runtime.removeOverride(core, ctr::native::kBootResourceWaitFirstCaller),
        "could not remove the resource-load continuation owner");

  // ------------------------------------------------------------ StartupResourcePump
  // Its decision: a false poll yields the field and keeps the phase, so the next field resumes the
  // SAME loop rather than starting a second one.
  ctr::StartupResourcePump pump(runtime, dmaCallbacks, field);
  field.beginField();
  check(!pump.isActive() && pump.phase() == ctr::StartupResourcePump::Phase::Inactive,
        "a fresh resource pump claimed a phase");
  check(runtime.installOverride(core, ctr::native::kBootResourcePumpReturn, "pump continuation", endTheField),
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
  check(runtime.removeOverride(core, ctr::native::kBootResourcePumpReturn),
        "could not remove the resource-pump continuation owner");

  // ------------------------------------------------------------ StartupAudioWait
  // Its decision: only the one measured callsite's service call is owned. A service call reached
  // from anywhere else is retail's, and the wait is owed only while the XA task is still running.
  ctr::StartupAudioWait audio(runtime, dmaCallbacks, field);
  field.beginField();
  installReturnSetting(core, ctr::native::kStartupAudioService, kSetV0One);
  check(runtime.installOverride(core, ctr::native::kStartupAudioLoop, "audio continuation", endTheField),
        "could not install the startup-audio continuation owner");
  // The loop's own body is retail. `callOriginalUntilExit` enters it with the owner's own override
  // SUPPRESSED, so the body has to reach the field boundary by itself; it reaches it the way retail
  // does, by calling the frame-timing owner.
  core.mem_w32(ctr::native::kStartupAudioLoop, jump(ctr::native::kFrameTiming));
  core.mem_w32(ctr::native::kStartupAudioLoop + 4u, 0u);
  check(runtime.installOverride(core, ctr::native::kFrameTiming, "timing boundary", endTheField),
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

  continuationsReached = 0;
  audio.resumeLoop(core);
  check(continuationsReached == 1u, "the startup audio loop did not run its retail body through to a field boundary");
  check(pendingIsFrameBoundary(core), "the startup audio loop did not end the field");
  clearPending(core);
  check(runtime.removeOverride(core, ctr::native::kFrameTiming), "could not remove the timing owner");
  check(runtime.removeOverride(core, ctr::native::kStartupAudioLoop), "could not remove the audio owner");

  std::printf("CTR field owners: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
