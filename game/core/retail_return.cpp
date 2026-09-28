#include "retail_return.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

void refuseUnexpectedRetailReturn(const char *owner, uint32_t expectedReturn, uint32_t actualReturn) {
  lucent::error("ctr-frame",
                "{} reached from 0x{:08X}; expected exact retail return 0x{:08X}",
                owner,
                actualReturn,
                expectedReturn);
  std::abort();
}

} // namespace ctr
