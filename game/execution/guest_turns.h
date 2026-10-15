#pragma once

#include "execution_exit.h"

#include <cstdint>
#include <string_view>

class Core;

namespace ctr::guest {

// The field turn: not a guest call, its terminal exit is the typed frame boundary.
psx::cpu::ExecutionResult dispatch(Core &core, uint32_t address);
// A state-zero body may outlive a display field, so this call is unbounded.
uint32_t callToContinuation(Core &core, uint32_t address, uint32_t returnPc, std::string_view owner);
// The one nested hop whose terminal exit may be a frame boundary; the caller keeps the exit.
psx::cpu::ExecutionResult dispatchHop(Core &core, uint32_t address, uint32_t returnPc);
void callToReturn(Core &core, uint32_t address, std::string_view owner);
void callOriginalToReturn(Core &core, uint32_t address, std::string_view owner);
// Any exit other than a frame boundary stops the run.
void propagateFrameBoundary(Core &core, const psx::cpu::ExecutionResult &result, std::string_view owner);

} // namespace ctr::guest
