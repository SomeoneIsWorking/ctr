#pragma once

#include "native_dispatch.h"

#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace ctr {

// One guest address owned for a field; the name shows up in override lookup diagnostics.
struct FieldOverrideBinding {
  uint32_t address = 0;
  const char *name = nullptr;
  psx::cpu::NativeFunction function = nullptr;
};

// A failed install or remove aborts: overrides must not outlive the field.
class FieldOverrideScope final {
public:
  FieldOverrideScope(Core &core, std::span<const FieldOverrideBinding> bindings);
  ~FieldOverrideScope();

  FieldOverrideScope(const FieldOverrideScope &) = delete;
  FieldOverrideScope &operator=(const FieldOverrideScope &) = delete;

  [[nodiscard]] std::size_t installedCount() const;

private:
  Core &core_;
  std::span<const FieldOverrideBinding> bindings_;
  std::size_t installedCount_ = 0;
};

} // namespace ctr
