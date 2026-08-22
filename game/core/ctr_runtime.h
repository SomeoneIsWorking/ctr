#pragma once

#include "game_runtime.h"

#include <cstdint>

namespace ctr {

// Process-lifetime owner of CTR's framework-facing behavior. CTR currently has only a bounded
// generated-code trace harness, so it derives the real runtime seam directly: no legacy config or
// callback bag exists to adapt. The independently validated trace target is configured once before
// Core construction and remains the only dispatch bootInit performs.
class CtrRuntime final : public GameRuntime {
public:
  using Dispatch = void (*)(Core *core, uint32_t address);

  CtrRuntime(Dispatch dispatch, uint32_t bootTarget);

  uint32_t bootTarget() const;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;

private:
  Dispatch dispatch_;
  const uint32_t bootTarget_;
};

} // namespace ctr
