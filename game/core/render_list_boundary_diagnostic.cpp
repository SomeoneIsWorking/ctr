#include "render_list_boundary_diagnostic.h"

#include "cfg.h"
#include "core.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

RenderListBoundaryDiagnostic *RenderListBoundaryDiagnostic::active_ = nullptr;

RenderListBoundaryDiagnostic::RenderListBoundaryDiagnostic(bool enabled) : enabled_(enabled) {}

bool RenderListBoundaryDiagnostic::enabled() const {
  return enabled_;
}

const RenderListObservation &RenderListBoundaryDiagnostic::latest() const {
  return latest_;
}

bool RenderListBoundaryDiagnostic::readableRam(uint32_t address, uint32_t bytes) {
  const uint32_t physical = address & 0x1FFFFFFFu;
  if (physical >= 0x800000u) {
    return false;
  }
  const uint32_t mirrorOffset = physical & 0x1FFFFFu;
  return mirrorOffset <= 0x200000u - bytes;
}

RenderListNodeChain RenderListBoundaryDiagnostic::scanNodeChain(Core &core, uint32_t head) {
  RenderListNodeChain chain{.head = head};
  std::array<uint32_t, kNodeLimit> visited{};
  uint32_t node = head;

  while (node != 0u) {
    if (!readableRam(node, kNodeListLinkOffset + sizeof(uint32_t))) {
      chain.invalidPointer = true;
      return chain;
    }
    for (uint32_t index = 0; index < chain.nodes; ++index) {
      if (visited[index] == node) {
        chain.cycle = true;
        return chain;
      }
    }
    if (chain.nodes == kNodeLimit) {
      chain.limitReached = true;
      return chain;
    }

    visited[chain.nodes++] = node;
    chain.tail = node;
    chain.tailLink = core.mem_r32(node + kNodeListLinkOffset);
    node = core.mem_r32(node + kNodeNextOffset);
  }

  chain.terminated = true;
  return chain;
}

void RenderListBoundaryDiagnostic::observePublication(Core &core, const RetailBody &retailBody) {
  if (!enabled_) {
    return;
  }
  if (!retailBody) {
    lucent::error("ctr-render-list", "publisher observation requires the retail body");
    std::abort();
  }
  if (publishedThisField_) {
    report(0u, "superseded by a later publisher call in the same field");
    disarmPairWatch(core);
  }

  const uint32_t gameState = core.r[4];
  if (!readableRam(gameState, kPublishedListOffset + sizeof(uint32_t))) {
    latest_ = {.gameState = gameState};
    lucent::debug("ctr-render-list", "publisher game-state=0x{:08X} is not readable guest RAM", gameState);
    retailBody(core);
    publishedThisField_ = true;
    return;
  }

  retailBody(core);

  latest_ = {};
  latest_.gameState = gameState;
  latest_.validGameState = true;
  latest_.list = core.mem_r32(gameState + kPublishedListOffset);
  latest_.validList = readableRam(latest_.list, kPairBytes);
  if (latest_.validList) {
    latest_.pairReturn = core.mem_r32(latest_.list);
    latest_.pairDescriptor = core.mem_r32(latest_.list + sizeof(uint32_t));
  }
  for (uint32_t index = 0; index < latest_.sourceLists.size(); ++index) {
    const uint32_t slot = gameState + kSourceListOffset + index * kSourceListStride;
    latest_.sourceLists[index] = scanNodeChain(core, core.mem_r32(slot));
  }
  publishedThisField_ = true;
  armPairWatch(core);
}

void RenderListBoundaryDiagnostic::armPairWatch(Core &core) {
  if (!latest_.validList) {
    return;
  }
  if (cfg_str("PSXPORT_WWATCH")) {
    latest_.pairWatchConfiguredElsewhere = true;
    return;
  }
  if (core.storeWatchCb != nullptr) {
    latest_.pairWatchBusy = true;
    return;
  }
  if (active_ && active_ != this) {
    latest_.pairWatchBusy = true;
    return;
  }

  active_ = this;
  watchCore_ = &core;
  core.storeWatchCb = recordPairStore;
  core.wwatch_arm(latest_.list, latest_.list + kPairBytes);
  latest_.pairWatchArmed = true;
}

void RenderListBoundaryDiagnostic::disarmPairWatch(Core &core) {
  if (!latest_.pairWatchArmed) {
    return;
  }
  if (&core != watchCore_ || core.storeWatchCb != recordPairStore || active_ != this) {
    lucent::error("ctr-render-list", "pair watch ownership changed before the diagnostic field ended");
    std::abort();
  }
  core.storeWatchCb = nullptr;
  core.wwatch_arm(0u, 0u);
  active_ = nullptr;
  watchCore_ = nullptr;
  latest_.pairWatchArmed = false;
}

void RenderListBoundaryDiagnostic::recordPairStore(Core *core, uint32_t address, uint32_t value, uint32_t width) {
  if (!active_ || core != active_->watchCore_) {
    return;
  }
  RenderListObservation &observation = active_->latest_;
  ++observation.pairStores;
  if (!observation.firstWriter.seen) {
    observation.firstWriter = {
        .address = address,
        .value = value,
        .width = width,
        .pc = core->pc,
        .returnAddress = core->r[31],
        .seen = true,
    };
  }
}

void RenderListBoundaryDiagnostic::report(uint32_t field, const char *reason) const {
  if (!latest_.validGameState) {
    lucent::debug("ctr-render-list", "field {}: publisher observation has no readable game-state ({})", field, reason);
    return;
  }
  lucent::debug("ctr-render-list",
                "field {}: publisher={} list={} valid-list={} pair=({:08X},{:08X}) pair-stores={} first-writer={} ({})",
                field,
                latest_.gameState,
                latest_.list,
                latest_.validList ? 1 : 0,
                latest_.pairReturn,
                latest_.pairDescriptor,
                latest_.pairStores,
                latest_.firstWriter.seen ? "seen" : "none",
                reason);
  for (uint32_t index = 0; index < latest_.sourceLists.size(); ++index) {
    const RenderListNodeChain &chain = latest_.sourceLists[index];
    lucent::debug("ctr-render-list",
                  "field {}: source[{}] head={:08X} nodes={} tail={:08X} tail+8={:08X} terminated={} cycle={} "
                  "invalid={} limit={}",
                  field,
                  index,
                  chain.head,
                  chain.nodes,
                  chain.tail,
                  chain.tailLink,
                  chain.terminated ? 1 : 0,
                  chain.cycle ? 1 : 0,
                  chain.invalidPointer ? 1 : 0,
                  chain.limitReached ? 1 : 0);
  }
  if (latest_.firstWriter.seen) {
    const RenderListFirstWriter &writer = latest_.firstWriter;
    lucent::debug("ctr-render-list",
                  "field {}: first pair writer addr={:08X} value={:08X} width={} pc={:08X} ra={:08X}",
                  field,
                  writer.address,
                  writer.value,
                  writer.width,
                  writer.pc,
                  writer.returnAddress);
  }
  if (!latest_.validList) {
    lucent::debug("ctr-render-list", "field {}: no pair-store watch: publisher returned a null or non-RAM list", field);
  } else if (latest_.pairWatchConfiguredElsewhere) {
    lucent::debug(
        "ctr-render-list", "field {}: no pair-store watch: PSXPORT_WWATCH already owns Core's watch range", field);
  } else if (latest_.pairWatchBusy) {
    lucent::debug("ctr-render-list", "field {}: no pair-store watch: another Core store observer is active", field);
  } else if (latest_.pairStores == 0u) {
    lucent::debug("ctr-render-list", "field {}: pair-store count is explicitly zero after publication", field);
  }
}

void RenderListBoundaryDiagnostic::finishField(Core &core, uint32_t field) {
  if (!enabled_) {
    return;
  }
  if (!publishedThisField_) {
    if (!reportedNoPublication_) {
      lucent::debug(
          "ctr-render-list", "field {}: publisher calls=0; no list, source-node, or pair-store observation", field);
      reportedNoPublication_ = true;
    }
    return;
  }

  if (latest_.validList) {
    latest_.pairReturn = core.mem_r32(latest_.list);
    latest_.pairDescriptor = core.mem_r32(latest_.list + sizeof(uint32_t));
  }
  report(field, "field boundary");
  disarmPairWatch(core);
  publishedThisField_ = false;
}

} // namespace ctr
