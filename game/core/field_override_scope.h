#pragma once

#include "native_dispatch.h"

#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace ctr {

class CtrRuntime;

// One guest address the frame driver takes ownership of for the length of a field, with the name it
// is installed under. The name reaches the framework's override lookup diagnostics, so it is the
// first thing a reader of an execution trace sees for that address.
struct FieldOverrideBinding {
  uint32_t address = 0;
  const char *name = nullptr;
  psx::cpu::NativeFunction function = nullptr;
  // True for a DEBUG-ONLY owner, which the field installs only when that diagnostic is enabled. This
  // is stated per row rather than inferred from the row's position, so adding, removing or
  // reordering a row cannot silently change which owner a non-observing field leaves out.
  bool diagnosticOnly = false;
};

// Installs the field's native overrides on construction and removes every one of them on scope exit.
//
// The window is the dynamic extent of exactly one `CtrFrameDriver::stepFrame`. An override left
// installed past that point would answer guest calls the driver no longer owns, and one removed early
// would let a translated block reach the guest body a native owner had replaced, so the two halves are
// bound to the same lifetime. A failure to install or remove is refused rather than reported: a field
// running with the wrong set of overrides is not a degraded field, it is a different program.
class FieldOverrideScope final {
public:
  FieldOverrideScope(CtrRuntime &runtime,
                     Core &core,
                     std::span<const FieldOverrideBinding> bindings,
                     bool diagnosticsEnabled);
  ~FieldOverrideScope();

  FieldOverrideScope(const FieldOverrideScope &) = delete;
  FieldOverrideScope &operator=(const FieldOverrideScope &) = delete;

  // How many bindings this field actually installed, which is the declared table size less the
  // debug-only rows that `diagnosticsEnabled` excluded.
  [[nodiscard]] std::size_t installedCount() const;

private:
  // The one rule both halves of the scope use to decide whether a row belongs to this field.
  [[nodiscard]] bool installs(const FieldOverrideBinding &binding) const;

  CtrRuntime &runtime_;
  Core &core_;
  std::span<const FieldOverrideBinding> bindings_;
  std::size_t installedCount_ = 0;
  bool diagnosticsEnabled_ = false;
};

} // namespace ctr
