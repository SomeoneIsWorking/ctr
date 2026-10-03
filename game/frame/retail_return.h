#pragma once

#include <cstdint>

namespace ctr {

// Every title bridge resumes on ONE measured retail return address, not "some" return. The guest
// reaches a bridge by a `jal` to a known entry from a known site, so a return address the title has
// no identity for means the guest is executing a path the transcription was not written against.
// This refusal is shared by every bridge owner so the rule is stated once rather than re-spelled
// (and weakened) per owner, and so the message names what was expected as well as what arrived.
[[noreturn]] void refuseUnexpectedRetailReturn(const char *owner, uint32_t expectedReturn, uint32_t actualReturn);

} // namespace ctr
