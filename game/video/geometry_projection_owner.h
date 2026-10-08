#pragma once

#include "view_projection_plan.h"

#include <cstdint>

class Core;

namespace ctr {

class ProjectionOwner;

// The guest writes H from 18 sites, so widening keys on the framework's per-Core pre-op GTE
// observer instead of the descriptor publication (0x80042910). A 4:3 run is inert.
class CtrGeometryProjectionOwner final {
public:
  // Centres are the raw 16.16 control-register words; the guest's two publication sites scale them differently.
  struct RetailTriple {
    int32_t centerX = 0;
    int32_t centerY = 0;
    uint32_t distance = 0;
  };

  // The plan is read from the projection owner, never latched here.
  void setProjectionOwner(const ProjectionOwner *owner) {
    projectionOwner_ = owner;
  }

  // Plain function pointer for the framework observer; `user` is the owner.
  static void onGteOp(Core *core, uint64_t ordinal, uint32_t guestPc, uint32_t instruction, void *user);

  [[nodiscard]] static RetailTriple readLiveTriple();

  // What one perspective transform did with the triple it found.
  enum class Outcome : uint8_t {
    Widened,
    AlreadyOwned, // the registers already held this owner's published H
    LeftAtRetail,
    PlanNotResolved,
    NoPlanOwner,
  };

  // Exposed for the focused test.
  Outcome applyOwnedPlan(Core &core, const RetailTriple &retail);

  void observe(Core &core, uint32_t instruction);

private:
  friend class ScopedGteProjectionObservation;

  const ProjectionOwner *projectionOwner_ = nullptr;
  // Makes the widening idempotent across the transforms run through one published triple.
  int32_t lastPublishedDistance_ = 0;
  bool publishedOnce_ = false;
};

class ScopedGteProjectionObservation final {
public:
  ScopedGteProjectionObservation(Core &core, CtrGeometryProjectionOwner &owner);
  ~ScopedGteProjectionObservation();

  ScopedGteProjectionObservation(const ScopedGteProjectionObservation &) = delete;
  ScopedGteProjectionObservation &operator=(const ScopedGteProjectionObservation &) = delete;

private:
  CtrGeometryProjectionOwner &owner_;
  Core *core_ = nullptr;
};

} // namespace ctr
