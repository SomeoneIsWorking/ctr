#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// State zero's resource continuation at 0x8002DD24, whose two retail do/while poll loops are split
// across host fields: one field is yielded after each false poll.
class StartupResourcePump final {
public:
  enum class Phase : uint8_t {
    Inactive,
    ResourcePoll,
    CommitPoll,
  };

  StartupResourcePump(CtrRuntime &runtime, FieldBoundary &field);

  [[nodiscard]] bool isActive() const;
  [[nodiscard]] Phase phase() const;

  void begin(Core &core);

  void resume(Core &core);

private:
  [[nodiscard]] bool serviceOwedWork(Core &core);
  void pollResources(Core &core);
  void pollCommit(Core &core);
  void finish(Core &core);

  CtrRuntime &runtime_;
  FieldBoundary &field_;
  Phase phase_ = Phase::Inactive;
};

} // namespace ctr
