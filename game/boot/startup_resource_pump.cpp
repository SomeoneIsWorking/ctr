#include "startup_resource_pump.h"

#include "core.h"
#include "ctr_runtime.h"
#include "dma_callback_owner.h"
#include "execution_services.h"
#include "field_boundary.h"
#include "native_ownership.h"
#include "retail_return.h"
#include "vsync_bridge.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

// The frame the continuation creates for itself. `kSavedReturnSlot` is a name rather than a guess
// because `finish()` validates what it reads back against the measured caller; the block slot is
// named for the argument it is passed as.
inline constexpr uint32_t kFrameBytes = 32u;
inline constexpr uint32_t kBeginBlockSlot = 16u;
inline constexpr uint32_t kSavedReturnSlot = 24u;

// A byte in the global block that the continuation clears before its first poll. The image has not
// been read to establish what this byte is, so it is named for what this code does to it and no
// role is claimed for it.
inline constexpr uint32_t kUnattributedClearedByteOffsetFromGp = 2248u;

// The two literal arguments the continuation passes. Neither has an established meaning: the image
// has not been read for the callees' parameter lists, so both are named as the argument they are.
inline constexpr uint32_t kUnattributedBeginFirstArgument = 33u;
inline constexpr uint32_t kUnattributedCommitFirstArgument = 28u;

// The return address each retail stage function is given: the instruction after its own call.
inline constexpr uint32_t kBeginReturn = 0x8002DD3Cu;
inline constexpr uint32_t kResourcePollReturn = 0x8002DD44u;
inline constexpr uint32_t kCommitReturn = 0x8002DD54u;
inline constexpr uint32_t kCommitPollReturn = 0x8002DD5Cu;

// Guest instruction counts. They are retail's: yielding between polls must not make a field look
// cheaper than retail ran it.
inline constexpr uint32_t kBeginCallInstructionCount = 6u;
inline constexpr uint32_t kCommitCallInstructionCount = kOmittedCallInstructionCount;
inline constexpr uint32_t kCommitPollCallInstructionCount = kOmittedCallInstructionCount;
inline constexpr uint32_t kSuffixInstructionCount = 4u;

} // namespace

StartupResourcePump::StartupResourcePump(CtrRuntime &runtime, DmaCallbackOwner &dmaCallbacks, FieldBoundary &field)
    : runtime_(runtime), dmaCallbacks_(dmaCallbacks), field_(field) {}

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

  // Exact 0x8002DD24..0x8002DD3C setup. The retail resource state machine remains authoritative;
  // only its host-starving do/while ownership moves into the finite driver.
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
  servicePendingWork(core);
  return field_.pending(core);
}

void StartupResourcePump::servicePendingWork(Core &core) {
  // A synchronous native transfer can become owed during the immediately preceding translated
  // resource poll. Deliver CTR's measured channel-4 callback before the generic direct-runtime path
  // consumes a completion for which it has no legacy callback-table view, then service all remaining
  // IRQ sources normally.
  dmaCallbacks_.serviceSpu(core, runtime_);
  if (dmaCallbacks_.hasPendingSpu(core)) {
    field_.request(core);
    return;
  }
  psx::cpu::servicePendingWork(core);
}

} // namespace ctr
