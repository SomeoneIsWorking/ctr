#include "projection_owner.h"

#include "core.h"

#include <cmath>
#include <cstdlib>
#include <lucent/log.h>

namespace ctr {
namespace {

constexpr uint32_t kScreenDistanceOffset = 0x18u;
constexpr uint32_t kWidthOffset = 0x20u;
constexpr uint32_t kHeightOffset = 0x22u;

int32_t signedHalf(uint16_t value) {
  return static_cast<int16_t>(value) / 2;
}

bool agrees(float actual, int32_t expected) {
  return std::fabs(actual - static_cast<float>(expected)) < 0.001f;
}

} // namespace

void ProjectionOwner::publish(Core &core, const RetailBody &retailBody) {
  if (!retailBody) {
    lucent::error("ctr-projection", "projection publication requires the preserved retail body");
    std::abort();
  }

  ProjectionPublication next{
      .source = core.r[4],
      .nativeWidth = static_cast<int16_t>(core.mem_r16(core.r[4] + kWidthOffset)),
      .nativeHeight = static_cast<int16_t>(core.mem_r16(core.r[4] + kHeightOffset)),
      .centerX = signedHalf(core.mem_r16(core.r[4] + kWidthOffset)),
      .centerY = signedHalf(core.mem_r16(core.r[4] + kHeightOffset)),
      .screenDistance = core.mem_r32(core.r[4] + kScreenDistanceOffset),
      .sequence = current_.sequence + 1u,
  };

  retailBody(core);

  const auto &published = core.rsub.projParams;
  if (!published.geomValid() || !agrees(published.geomOfx(), next.centerX) ||
      !agrees(published.geomOfy(), next.centerY) || !agrees(published.geomH(), next.screenDistance)) {
    lucent::error("ctr-projection",
                  "retail projection publication disagreed with view 0x{:08X}: expected ({},{},H={}), got "
                  "({},{},H={})",
                  next.source,
                  next.centerX,
                  next.centerY,
                  next.screenDistance,
                  published.geomOfx(),
                  published.geomOfy(),
                  published.geomH());
    std::abort();
  }

  previous_ = current_;
  current_ = next;
}

const ProjectionPublication &ProjectionOwner::previous() const {
  return previous_;
}

const ProjectionPublication &ProjectionOwner::current() const {
  return current_;
}

} // namespace ctr
