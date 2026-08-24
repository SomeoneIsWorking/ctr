// CTR's shipping process entry composes the title runtime with psxport's machine services.
#include "bootstrap_frontier.h"
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "hw_bind.h"
#include "recomp_register.h"

#include <filesystem>
#include <lucent/log.h>
#include <memory>

extern "C" {
void mdec_init();
void spu_init();
void watchdog_init();
}

void gte_init();
void load_exe(const char *path, Core *core);
void rec_dispatch(Core *core, uint32_t address);

namespace {

constexpr const char *kDefaultExecutable = "scratch/raw/ctr/SCUS_944.26";
constexpr uint32_t kMeasuredEntry = 0x8007793Cu;

} // namespace

int main(int argc, char **argv) {
  if (argc != 1) {
    lucent::error("boot", "ctr_port takes no executable override; run ./run.sh with the verified CTR USA disc");
    return 2;
  }
  const char *executable = kDefaultExecutable;
  if (!std::filesystem::is_regular_file(executable)) {
    lucent::error("boot", "{} is absent; provision the verified CTR USA executable first", executable);
    return 2;
  }

  ctr::installRecompiledProgram();
  static ctr::CtrRuntime runtime(rec_dispatch, kMeasuredEntry);
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  Core *core = &game->core;
  watchdog_init();
  load_exe(executable, core);

  gte_init();
  mdec_init();
  spu_init();
  gte_bind(core);
  core->rsub.projprim.bind(core);
  spu_bind(core);
  mdec_bind(core);
  xa_bind(core);
  game->spu_audio.init();
  game->gpu.gpu_native_init();
  game->pad.overridesInit();
  core->runtime->registerOverrides(*game);

  lucent::info("boot", "entering CTR at measured executable entry 0x{:08X}", kMeasuredEntry);
  const ctr::BootstrapResult result = ctr::runBootstrapToSupportedFrontier(runtime, *core);
  if (result != ctr::BootstrapResult::ReachedSupportedFrontier) {
    lucent::error("boot", "CTR returned before reaching its supported bootstrap frontier");
    return 2;
  }
  lucent::info("boot", "CTR bootstrap reached the current supported boundary; gameplay is not available yet");
  return 0;
}
