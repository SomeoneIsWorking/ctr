#include "execution/platform_leaves.h"

#include "core.h"
#include "disc/async_disc_owner.h"
#include "game.h"
#include "platform_hle.h"
#include "runtime/runtime.h"
#include "title/native_ownership.h"

#include <lucent/log.h>

#include <cstdlib>

namespace ctr {
namespace {

constexpr uint32_t kGpuTimeoutFields = 240u;
constexpr uint32_t kGpuTimeoutPollLimit = 0xF0000u;
constexpr uint32_t kCdSyncResultBytes = 8u;

} // namespace

void gpuTimeoutArm(Core *core) {
  // 0x800750A8 minus the VSync(-1) query; the host loop advances Timing::vblank as the same clock.
  const uint32_t deadline = core->game->timing.vblank + kGpuTimeoutFields;
  core->mem_w32(native::kGpuTimeoutDeadline, deadline);
  core->mem_w32(native::kGpuTimeoutPollCount, 0u);
  core->r[2] = deadline;
}

void gpuTimeoutCheck(Core *core) {
  const uint32_t deadline = core->mem_r32(native::kGpuTimeoutDeadline);
  const uint32_t pollCount = core->mem_r32(native::kGpuTimeoutPollCount);
  const bool fieldExpired = static_cast<int32_t>(deadline) < static_cast<int32_t>(core->game->timing.vblank);
  if (!fieldExpired) {
    core->mem_w32(native::kGpuTimeoutPollCount, pollCount + 1u);
  }
  if (fieldExpired || pollCount > kGpuTimeoutPollLimit) {
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
    for (uint32_t offset = 0; offset < kCdSyncResultBytes; ++offset) {
      core->mem_w8(result + offset, 0u);
    }
  }
  core->r[2] = native::kCdlComplete;
}

void bindPlatformLeaves(Game &game) {
  PlatformHle &table = game.platform_hle;
  table.bindStock(native::kGpuTimeoutArm, gpuTimeoutArm);
  table.bindStock(native::kGpuTimeoutCheck, gpuTimeoutCheck);
  table.bindStock(native::kCdRead, readWithCompletionCallback);
  table.bindStock(native::kCdSync, cdSync);
}

} // namespace ctr
