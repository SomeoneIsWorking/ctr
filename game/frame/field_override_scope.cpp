#include "field_override_scope.h"

#include "core.h"
#include "ctr_runtime.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

FieldOverrideScope::FieldOverrideScope(Core &core, std::span<const FieldOverrideBinding> bindings)
    : core_(core), bindings_(bindings) {
  for (const FieldOverrideBinding &binding : bindings_) {
    if (!psx::cpu::tryInstallNativeOverride(core_, binding.address, binding.name, binding.function).has_value()) {
      lucent::error("ctr-frame", "could not install '{}' at 0x{:08X}", binding.name, binding.address);
      std::abort();
    }
    ++installedCount_;
  }
}

FieldOverrideScope::~FieldOverrideScope() {
  for (const FieldOverrideBinding &binding : bindings_) {
    if (!psx::cpu::removeNativeOverride(core_, binding.address)) {
      lucent::error("ctr-frame", "could not remove '{}' at 0x{:08X}", binding.name, binding.address);
      std::abort();
    }
  }
}

std::size_t FieldOverrideScope::installedCount() const {
  return installedCount_;
}

} // namespace ctr
