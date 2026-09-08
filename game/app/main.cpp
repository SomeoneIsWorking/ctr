// CTR's shipping process entry composes the title runtime with psxport's machine services.
#include "command_line.h"
#include "core.h"
#include "ctr_runtime.h"
#include "frame_loop_shell.h"
#include "game.h"
#include "hw_bind.h"
#include "lightrec_executor.h"
#include "native_ownership.h"
#include "psx_exe_image.h"
#include "runtime_composition.h"

#include <filesystem>
#include <iostream>
#include <lucent/log.h>
#include <memory>

extern "C" {
void mdec_init();
void spu_init();
void watchdog_init();
}

namespace {

constexpr const char *kDefaultExecutable = "scratch/raw/ctr/SCUS_944.26";
} // namespace

int main(int argc, char **argv) {
  const ctr::CommandLineAction action = ctr::parseCommandLine(argc, argv);
  if (action == ctr::CommandLineAction::help) {
    ctr::printUsage(std::cout, argv[0]);
    return 0;
  }
  if (action == ctr::CommandLineAction::invalid) {
    lucent::error("boot", "ctr_port takes no executable override; run ./run.sh with the verified CTR USA disc");
    ctr::printUsage(std::cout, argv[0]);
    return 2;
  }
  const char *executable = kDefaultExecutable;
  if (!std::filesystem::is_regular_file(executable)) {
    lucent::error("boot", "{} is absent; provision the verified CTR USA executable first", executable);
    return 2;
  }

  static ctr::CtrRuntime runtime(ctr::native::kExecutableEntry);
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  Core *core = &game->core;
  watchdog_init();
  load_exe(executable, core);
  if (!core->lightrecExecutor().available()) {
    lucent::error("boot", "psxport was built without its Lightrec dynarec backend");
    return 2;
  }

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
  ctr::installRuntimeOwners(*game);

  lucent::info("boot", "entering CTR at measured executable entry 0x{:08X}", ctr::native::kExecutableEntry);
  FrameLoopShell shell;
  shell.prepareProduct(*game);
  for (uint32_t frame = 0;; ++frame) {
    shell.step(*core, frame);
  }
}
