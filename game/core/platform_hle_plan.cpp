#include "platform_hle_plan.h"

#include "cd_control.h"
#include "core.h"
#include "game.h"
#include "native_ownership.h"
#include "platform_hle.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

constexpr uint32_t kGpuTimeoutFields = 240u;
constexpr uint32_t kGpuTimeoutPollLimit = 0xF0000u;

void gpuTimeoutArm(Core *core) {
  if (!core->game) {
    lucent::error("ctr-gpu-timeout", "libgpu timeout arm has no bound Game field owner");
    std::abort();
  }
  // Exact observable state of SCUS_944.26 0x800750A8, except the VSync(-1) query. The host frame
  // loop advances Timing::vblank, so this is the same clock under native ownership.
  const uint32_t deadline = core->game->timing.vblank + kGpuTimeoutFields;
  core->mem_w32(native::kGpuTimeoutDeadline, deadline);
  core->mem_w32(native::kGpuTimeoutPollCount, 0u);
  core->r[2] = deadline;
}

void gpuTimeoutCheck(Core *core) {
  if (!core->game) {
    lucent::error("ctr-gpu-timeout", "libgpu timeout check has no bound Game field owner");
    std::abort();
  }
  const uint32_t deadline = core->mem_r32(native::kGpuTimeoutDeadline);
  const uint32_t pollCount = core->mem_r32(native::kGpuTimeoutPollCount);
  const bool fieldExpired = static_cast<int32_t>(deadline) < static_cast<int32_t>(core->game->timing.vblank);
  if (!fieldExpired) {
    core->mem_w32(native::kGpuTimeoutPollCount, pollCount + 1u);
  }
  if (fieldExpired || pollCount > kGpuTimeoutPollLimit) {
    // A timeout contradicts the synchronous native-GPU contract. Do not pretend recovery succeeded;
    // stop at the producer which failed to drain rather than re-entering the retail hardware reset.
    lucent::error("ctr-gpu-timeout",
                  "native GPU queue failed to drain: field={} deadline={} polls={}",
                  core->game->timing.vblank,
                  deadline,
                  pollCount);
    std::abort();
  }
  core->r[2] = 0u;
}

void cdSync(Core *core) {
  const uint32_t result = core->r[5];
  if (result != 0) {
    for (uint32_t offset = 0; offset < 8u; ++offset) {
      core->mem_w8(result + offset, 0u);
    }
  }
  core->r[2] = 2u;
}

void cdControl(Core *core) {
  cd_control_sync(core);
  // CTR's 0x8007BC38 contract returns zero on success; cd_control_sync's shared blocking-control
  // contract returns one. Command side effects and result bytes remain owned by the shared model.
  core->r[2] = 0u;
}

const PlatformHlePlan kPlatformHlePlan{
    .setGeomOffset = native::kSetGeomOffset,
    .setGeomScreen = native::kSetGeomScreen,
    .vsyncAddress = native::kVSync,
    .bindings = {{native::kGpuTimeoutArm, gpuTimeoutArm},
                 {native::kGpuTimeoutCheck, gpuTimeoutCheck},
                 {native::kCdRead, cd_read_stock_sync},
                 {native::kCdReadSync, cd_readsync_stock_sync},
                 {native::kCdSync, cdSync},
                 {native::kCdControl, cdControl}},
    .bindingCount = 6,
    .windowLo = {native::kGpuTimeoutArm, native::kCdSync},
    .windowHi = {native::kProjectionWindowEnd, native::kCdControlWindowEnd},
};

} // namespace

const PlatformHlePlan &platformHlePlan() {
  return kPlatformHlePlan;
}

} // namespace ctr
