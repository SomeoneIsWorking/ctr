#include "startup_resource_load.h"

#include "core.h"
#include "ctr_runtime.h"
#include "execution_services.h"
#include "field_boundary.h"
#include "native_ownership.h"
#include "retail_return.h"
#include "vsync_bridge.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// Frame the -1 branch of 0x80031FDC creates; the spill order is not re-derived from the image.
inline constexpr uint32_t kFrameBytes = 72u;
inline constexpr uint32_t kSavedS0Slot = 48u;
inline constexpr uint32_t kSavedS1Slot = 52u;
inline constexpr uint32_t kSavedS2Slot = 56u;
inline constexpr uint32_t kSavedS3Slot = 60u;
inline constexpr uint32_t kSavedS4Slot = 64u;
inline constexpr uint32_t kSavedReturnSlot = 68u;

// The caller-frame fifth argument that selects this branch; the spill reads it through the new frame pointer.
inline constexpr uint32_t kCallerFifthArgumentSlot = 16u;
inline constexpr uint32_t kCallerFifthArgumentThroughNewFrameSlot = kFrameBytes + kCallerFifthArgumentSlot;
inline constexpr uint32_t kSynchronousFifthArgument = 0xFFFFFFFFu;

// The six-word block passed as a0 to both the setup and commit calls; only the setup result word has a known role.
inline constexpr uint32_t kBlockWord0Slot = 24u;
inline constexpr uint32_t kBlockWord1LowHalfSlot = 28u;
inline constexpr uint32_t kBlockWord1HighHalfSlot = 30u;
inline constexpr uint32_t kBlockWord2Slot = 32u;
inline constexpr uint32_t kBlockWord3SetupResultSlot = 36u;
inline constexpr uint32_t kBlockWord4Slot = 40u;
inline constexpr uint32_t kBlockWord5Slot = 44u;

// Two words of the new frame that are written and never read in this branch.
inline constexpr uint32_t kUnattributedSpilledWordSlot = 16u;
inline constexpr uint32_t kUnattributedZeroedWordSlot = 20u;

// The retail helpers this branch runs through Lightrec and the return address given to each.
inline constexpr uint32_t kResourceSetupCall = native::kBootResourceSetup;
inline constexpr uint32_t kResourceSetupReturn = 0x80032054u;
inline constexpr uint32_t kResourceCommitCall = native::kBootResourceCommit;
inline constexpr uint32_t kResourceCommitReturn = 0x8003206Cu;
inline constexpr uint32_t kPreWaitHelperCall = native::kBootResourcePreWaitHelper;
inline constexpr uint32_t kPreWaitHelperReturn = 0x80032018u;

// v0 is loaded twice before the setup call; both stores are kept and the second survives.
inline constexpr uint32_t kFirstLoadedValue = 0xFFFFFFFFu;
inline constexpr uint32_t kSecondLoadedValue = 0xFFFFFFFEu;
inline constexpr uint32_t kSetupSecondArgument = 3u;

// Guest instruction counts, matched to the transcribed sequences.
inline constexpr uint32_t kPrologueInstructionCount = 13u;
inline constexpr uint32_t kArgumentSetupInstructionCount = 3u;
inline constexpr uint32_t kSetupCallInstructionCount = 12u;
inline constexpr uint32_t kCommitCallInstructionCount = 6u;

// The omitted call is VSync(2); ra and a0 are left as the guest's call would leave them.
inline constexpr uint32_t kOmittedVSyncMode = 2u;
inline constexpr uint32_t kOmittedVSyncFields = 1u;

} // namespace

StartupResourceLoad::StartupResourceLoad(CtrRuntime &runtime, FieldBoundary &field)
    : runtime_(runtime), field_(field) {}

bool StartupResourceLoad::ownsSuffix() const {
  return suffixAddress_ != 0;
}

bool StartupResourceLoad::consumeWaitedField() {
  if (waitedFields_ == 0) {
    return false;
  }
  --waitedFields_;
  return true;
}

void StartupResourceLoad::begin(Core &core) {
  const uint32_t callerStack = core.r[29];
  if (core.mem_r32(callerStack + kCallerFifthArgumentSlot) != kSynchronousFifthArgument) {
    runtime_.callOriginalToReturn(core, native::kBootResourceWait, "CTR boot-resource wait");
    return;
  }
  if (ownsSuffix() || waitedFields_ != 0) {
    lucent::error("ctr-frame", "nested state-zero resource waits are not supported");
    std::abort();
  }

  // 0x80031FDC..0x80032074 for fifth argument -1; helper calls run through Lightrec.
  const uint32_t frame = callerStack - kFrameBytes;
  core.r[29] = frame;
  const auto storeWord = [&core, frame](uint32_t slot, uint32_t value) {
    core.mem_w32(frame + slot, value);
  };
  const auto storeHalf = [&core, frame](uint32_t slot, uint16_t value) {
    core.mem_w16(frame + slot, value);
  };

  storeWord(kSavedS1Slot, core.r[17]);
  core.r[17] = core.mem_r32(frame + kCallerFifthArgumentThroughNewFrameSlot);
  storeWord(kSavedS0Slot, core.r[16]);
  core.r[16] = core.r[4];
  storeWord(kSavedS2Slot, core.r[18]);
  core.r[18] = core.r[5];
  storeWord(kSavedS4Slot, core.r[20]);
  core.r[20] = core.r[6];
  storeWord(kSavedS3Slot, core.r[19]);
  core.r[19] = core.r[7];
  storeWord(kSavedReturnSlot, core.r[31]);
  psx::cpu::accountGuestInstructions(core, kPrologueInstructionCount);

  if (core.r[20] == 0) {
    core.r[31] = kPreWaitHelperReturn;
    psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
    runtime_.callToReturn(core, kPreWaitHelperCall, "CTR pre-resource-wait helper");
  }

  core.r[2] = kFirstLoadedValue;
  core.r[2] = kSecondLoadedValue;
  psx::cpu::accountGuestInstructions(core, kArgumentSetupInstructionCount);
  core.r[4] = core.r[16];
  core.r[5] = kSetupSecondArgument;
  core.r[6] = core.r[18];
  core.r[7] = core.r[20];
  core.r[2] = core.r[5];
  storeWord(kBlockWord0Slot, core.r[4]);
  storeHalf(kBlockWord1LowHalfSlot, 0u);
  storeHalf(kBlockWord1HighHalfSlot, static_cast<uint16_t>(core.r[2]));
  storeWord(kBlockWord2Slot, core.r[6]);
  storeWord(kUnattributedSpilledWordSlot, core.r[19]);
  core.r[31] = kResourceSetupReturn;
  storeWord(kUnattributedZeroedWordSlot, 0u);
  psx::cpu::accountGuestInstructions(core, kSetupCallInstructionCount);
  runtime_.callToReturn(core, kResourceSetupCall, "CTR resource setup");

  storeWord(kBlockWord3SetupResultSlot, core.r[2]);
  core.r[2] = core.mem_r32(core.r[19]);
  core.r[4] = frame + kBlockWord0Slot;
  storeWord(kBlockWord5Slot, 0u);
  core.r[31] = kResourceCommitReturn;
  storeWord(kBlockWord4Slot, core.r[2]);
  psx::cpu::accountGuestInstructions(core, kCommitCallInstructionCount);
  runtime_.callToReturn(core, kResourceCommitCall, "CTR resource commit");

  core.r[31] = native::kBootResourceWaitReturn;
  core.r[4] = kOmittedVSyncMode;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  suffixAddress_ = native::kBootResourceWaitReturn;
  waitedFields_ = kOmittedVSyncFields;
  field_.request(core);
}

void StartupResourceLoad::resume(Core &core) {
  const uint32_t suffix = suffixAddress_;
  suffixAddress_ = 0;
  // Incoming ra names the interior VSync continuation, not the suffix's return boundary.
  const uint32_t caller = core.mem_r32(core.r[29] + kSavedReturnSlot);
  if (caller != native::kBootResourceWaitFirstCaller && caller != native::kBootResourceWaitSecondCaller &&
      caller != native::kBootResourceWaitRaceCaller) {
    refuseUnexpectedRetailReturn("state-zero resource wait", native::kBootResourceWaitFirstCaller, caller);
  }
  runtime_.callToContinuation(core, suffix, caller, "CTR resource-wait suffix");
  if (caller != native::kBootResourceWaitRaceCaller) {
    runtime_.propagateFrameBoundary(core, runtime_.dispatch(core, caller), "CTR resource continuation");
    return;
  }
  // The race/menu caller is an interior suffix of 0x80033610 and returns to its sole direct caller.
  const auto raceCaller = runtime_.dispatchHop(core, caller, native::kBootResourceWaitRaceResume);
  if (!raceCaller.returned()) {
    runtime_.propagateFrameBoundary(core, raceCaller, "CTR race resource caller");
    return;
  }
  if (core.r[31] != native::kBootResourceWaitRaceResume) {
    refuseUnexpectedRetailReturn("state-zero race resource suffix", native::kBootResourceWaitRaceResume, core.r[31]);
  }
  runtime_.propagateFrameBoundary(
      core, runtime_.dispatch(core, native::kBootResourceWaitRaceResume), "CTR race resource continuation");
}

} // namespace ctr
