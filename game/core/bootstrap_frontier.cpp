#include "bootstrap_frontier.h"

#include "core.h"
#include "ctr_runtime.h"
#include "recomp_register.h"

#include <lucent/log.h>

namespace ctr {
namespace {

constexpr uint32_t kSupportedFrontier = 0x800772E0u;

struct FrontierReached final {};

void stopAtSupportedFrontier(Core *) {
  lucent::info("boot", "reached supported CTR bootstrap frontier 0x{:08X}", kSupportedFrontier);
  throw FrontierReached{};
}

class FrontierOverride final {
public:
  FrontierOverride() {
    setRecompiledOverride(kSupportedFrontier, stopAtSupportedFrontier);
  }

  ~FrontierOverride() {
    setRecompiledOverride(kSupportedFrontier, nullptr);
  }

  FrontierOverride(const FrontierOverride &) = delete;
  FrontierOverride &operator=(const FrontierOverride &) = delete;
};

} // namespace

BootstrapResult runBootstrapToSupportedFrontier(CtrRuntime &runtime, Core &core) {
  FrontierOverride boundary;
  try {
    runtime.bootInit(core);
  } catch (const FrontierReached &) {
    return BootstrapResult::ReachedSupportedFrontier;
  }
  return BootstrapResult::EntryReturned;
}

} // namespace ctr
