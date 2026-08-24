#include "recomp_register.h"

#include "overlay_table.h"
#include "recomp_iface.h"

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

} // namespace ctr
