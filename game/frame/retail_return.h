#pragma once

#include <cstdint>

namespace ctr {

// A resume from any other retail return address is refused naming both.
[[noreturn]] void refuseUnexpectedRetailReturn(const char *owner, uint32_t expectedReturn, uint32_t actualReturn);

} // namespace ctr
