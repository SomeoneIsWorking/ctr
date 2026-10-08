#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// State zero's startup resource load at 0x80031FDC with its fifth-argument -1 branch's VSync(2) removed.
class StartupResourceLoad final {
public:
  StartupResourceLoad(CtrRuntime &runtime, FieldBoundary &field);

  [[nodiscard]] bool ownsSuffix() const;

  [[nodiscard]] bool consumeWaitedField();

  // Any other argument runs the retail body and owns nothing.
  void begin(Core &core);

  void resume(Core &core);

private:
  CtrRuntime &runtime_;
  FieldBoundary &field_;
  uint32_t suffixAddress_ = 0;
  uint32_t waitedFields_ = 0;
};

} // namespace ctr
