#include "field_override_scope.h"

#include "core.h"
#include "ctr_runtime.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

FieldOverrideScope::FieldOverrideScope(Core &core,
                                       std::span<const FieldOverrideBinding> bindings,
                                       bool diagnosticsEnabled)
    : core_(core), bindings_(bindings), diagnosticsEnabled_(diagnosticsEnabled) {
  for (const FieldOverrideBinding &binding : bindings_) {
    if (!installs(binding)) {
      continue;
    }
    if (!psx::cpu::tryInstallNativeOverride(core_, binding.address, binding.name, binding.function).has_value()) {
      lucent::error("ctr-frame", "could not install '{}' at 0x{:08X}", binding.name, binding.address);
      std::abort();
    }
    ++installedCount_;
  }
}

// Removes exactly the rows the constructor installed, chosen by the same predicate. Iterating the
// whole table and asking the framework to remove a row that was never installed would be a refusal,
// not a cleanup, so the two halves cannot be allowed to disagree about which rows those are.
FieldOverrideScope::~FieldOverrideScope() {
  for (const FieldOverrideBinding &binding : bindings_) {
    if (!installs(binding)) {
      continue;
    }
    if (!psx::cpu::removeNativeOverride(core_, binding.address)) {
      lucent::error("ctr-frame", "could not remove '{}' at 0x{:08X}", binding.name, binding.address);
      std::abort();
    }
  }
}

std::size_t FieldOverrideScope::installedCount() const {
  return installedCount_;
}

bool FieldOverrideScope::installs(const FieldOverrideBinding &binding) const {
  return diagnosticsEnabled_ || !binding.diagnosticOnly;
}

} // namespace ctr
