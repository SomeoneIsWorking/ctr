#pragma once

class Core;
class Game;

namespace ctr {

// libgpu's DMA-queue timeout pair on the host field clock: a timeout contradicts the synchronous GPU and stops the run.
void gpuTimeoutArm(Core *core);
void gpuTimeoutCheck(Core *core);
// CdSync as CdInit's callers read it: zeroed result bytes.
void cdSync(Core *core);

// Binds the leaves above and the title's CdRead at their SCUS_944.26 addresses.
void bindPlatformLeaves(Game &game);

} // namespace ctr
