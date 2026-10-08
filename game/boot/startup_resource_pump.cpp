#include "startup_resource_pump.h"

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

// Frame the continuation creates; `finish()` checks the saved return against the retail caller.
inline constexpr uint32_t kFrameBytes = 32u;
inline constexpr uint32_t kBeginBlockSlot = 16u;
inline constexpr uint32_t kSavedReturnSlot = 24u;

// Global-block byte cleared before the first poll; its role is unidentified.
inline constexpr uint32_t kUnattributedClearedByteOffsetFromGp = 2248u;

// Two literal arguments the continuation passes; their meaning is unidentified.
inline constexpr uint32_t kUnattributedBeginFirstArgument = 33u;
inline constexpr uint32_t kUnattributedCommitFirstArgument = 28u;

// The return address given to each retail stage function.
inline constexpr uint32_t kBeginReturn = 0x8002DD3Cu;
inline constexpr uint32_t kResourcePollReturn = 0x8002DD44u;
inline constexpr uint32_t kCommitReturn = 0x8002DD54u;
inline constexpr uint32_t kCommitPollReturn = 0x8002DD5Cu;

// Guest instruction counts, matching retail.
inline constexpr uint32_t kBeginCallInstructionCount = 6u;
inline constexpr uint32_t kCommitCallInstructionCount = kOmittedCallInstructionCount;
inline constexpr uint32_t kCommitPollCallInstructionCount = kOmittedCallInstructionCount;
inline constexpr uint32_t kSuffixInstructionCount = 4u;

} // namespace

StartupResourcePump::StartupResourcePump(CtrRuntime &runtime, FieldBoundary &field)
    : runtime_(runtime), field_(field) {}

bool StartupResourcePump::isActive() const {
  return phase_ != Phase::Inactive;
}

StartupResourcePump::Phase StartupResourcePump::phase() const {
  return phase_;
}

void StartupResourcePump::begin(Core &core) {
  if (core.r[31] != native::kBootResourcePumpReturn) {
    refuseUnexpectedRetailReturn("state-zero resource pump", native::kBootResourcePumpReturn, core.r[31]);
  }
  if (isActive()) {
    lucent::error("ctr-frame", "nested state-zero resource pumps are not supported");
    std::abort();
  }

  // 0x8002DD24..0x8002DD3C setup; the retail state machine stays authoritative.
  const uint32_t frame = core.r[29] - kFrameBytes;
  core.r[29] = frame;
  core.r[4] = kUnattributedBeginFirstArgument;
  core.mem_w32(frame + kSavedReturnSlot, core.r[31]);
  core.mem_w8(core.r[28] + kUnattributedClearedByteOffsetFromGp, 0u);
  core.r[31] = kBeginReturn;
  core.r[5] = frame + kBeginBlockSlot;
  psx::cpu::accountGuestInstructions(core, kBeginCallInstructionCount);
  runtime_.callToReturn(core, native::kBootResourcePumpBegin, "CTR resource-pump begin");
  phase_ = Phase::ResourcePoll;
  resume(core);
}

void StartupResourcePump::resume(Core &core) {
  if (phase_ == Phase::ResourcePoll) {
    pollResources(core);
    if (phase_ != Phase::CommitPoll) {
      return;
    }
  }
  if (phase_ != Phase::CommitPoll) {
    lucent::error("ctr-frame", "state-zero resource pump resumed without a measured phase");
    std::abort();
  }
  if (serviceOwedWork(core)) {
    return;
  }
  pollCommit(core);
  if (core.r[2] == 0u) {
    field_.request(core);
    return;
  }
  finish(core);
}

void StartupResourcePump::pollResources(Core &core) {
  if (serviceOwedWork(core)) {
    return;
  }
  core.r[31] = kResourcePollReturn;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  runtime_.callToReturn(core, native::kBootResourcePumpPoll, "CTR resource-pump poll");
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  if (core.r[2] == 0u) {
    field_.request(core);
    return;
  }
  core.r[31] = kCommitReturn;
  core.r[4] = kUnattributedCommitFirstArgument;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  runtime_.callToReturn(core, native::kBootResourcePumpCommit, "CTR resource-pump commit");
  phase_ = Phase::CommitPoll;
}

void StartupResourcePump::pollCommit(Core &core) {
  core.r[31] = kCommitPollReturn;
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
  runtime_.callToReturn(core, native::kBootResourcePumpCommitPoll, "CTR resource-pump commit poll");
  psx::cpu::accountGuestInstructions(core, kOmittedCallInstructionCount);
}

void StartupResourcePump::finish(Core &core) {
  const uint32_t continuation = core.mem_r32(core.r[29] + kSavedReturnSlot);
  core.r[31] = continuation;
  core.r[29] += kFrameBytes;
  psx::cpu::accountGuestInstructions(core, kSuffixInstructionCount);
  phase_ = Phase::Inactive;
  if (continuation != native::kBootResourcePumpReturn) {
    refuseUnexpectedRetailReturn("state-zero resource-pump suffix", native::kBootResourcePumpReturn, continuation);
  }
  runtime_.propagateFrameBoundary(core, runtime_.dispatch(core, continuation), "CTR resource-pump continuation");
}

bool StartupResourcePump::serviceOwedWork(Core &core) {
  if (!core.pending_work) {
    return false;
  }
  psx::cpu::servicePendingWork(core);
  return field_.pending(core);
}

} // namespace ctr
