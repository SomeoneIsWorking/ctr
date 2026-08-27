#pragma once

#include "ctr_runtime.h"

namespace ctr {

using RecompiledOverride = void (*)(Core *);

// Install the generated resident substrate before constructing a Game. The product entry point
// reaches generated symbols only through this narrow adapter.
void installRecompiledProgram();
void setRecompiledOverride(uint32_t address, RecompiledOverride overrideFunction);
void runRecompiledSuper(Core *core, uint32_t address);

} // namespace ctr
