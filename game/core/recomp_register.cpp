#include "recomp_register.h"

#include "native_ownership.h"
#include "overlay_table.h"
#include "rec_decls.h"
#include "recomp_iface.h"

#include <cstdlib>
#include <lucent/log.h>

int rec_func_index(uint32_t address);
void shard_set_override(uint32_t address, void (*overrideFunction)(Core *));

namespace ctr {
namespace {

const RecompRegistry kCtrRecompiledProgram{
    .main_dispatch = main_dispatch,
    .rec_func_index = rec_func_index,
    .overlays = g_rec_overlays,
    .overlay_count = g_rec_overlay_count,
    .shard_set_override = shard_set_override,
};

} // namespace

void installRecompiledProgram() {
  psxport_install_recomp(&kCtrRecompiledProgram);
}

void setRecompiledOverride(uint32_t address, RecompiledOverride overrideFunction) {
  shard_set_override(address, overrideFunction);
}

void runRecompiledSuper(Core *core, uint32_t address) {
  switch (address) {
  case native::kStartupGpuInit:
    gen_func_8003D7D8(core);
    return;
  case native::kStartupDisplayInit:
    gen_func_800251AC(core);
    return;
  case native::kBootResourceWait:
    gen_func_80031FDC(core);
    return;
  case native::kStartupAudioService:
    gen_func_8001D06C(core);
    return;
  case native::kStartupAudioLoop:
    gen_func_8003C94C(core);
    return;
  case native::kShutdownDisplay:
    gen_func_80025208(core);
    return;
  case native::kVblankCallbackInstall:
    gen_func_80077254(core);
    return;
  case native::kFrameTiming:
    gen_func_8004B3A4(core);
    return;
  case native::kProjectionProducer:
    gen_func_80042910(core);
    return;
  case native::kRenderListPublisher:
    gen_func_8003B43C(core);
    return;
  default:
    lucent::error("ctr-recomp", "no preserved generated super for 0x{:08X}", address);
    std::abort();
  }
}

} // namespace ctr
