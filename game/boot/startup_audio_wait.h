#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// State zero's post-archive XA wait on the task at 0x8008D708: serves the retail service call,
// yields one host field per call and resumes at the retail poll.
class StartupAudioWait final {
public:
  StartupAudioWait(CtrRuntime &runtime, FieldBoundary &field);

  [[nodiscard]] bool ownsContinuation() const;

  void service(Core &core);

  void resume(Core &core);

private:
  CtrRuntime &runtime_;
  FieldBoundary &field_;
  uint32_t continuationAddress_ = 0;
};

} // namespace ctr
