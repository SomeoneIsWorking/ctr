#include "c_subsys.h"
#include "command_line.h"
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "lightrec_executor.h"
#include "machine.h"
#include "native_ownership.h"
#include "psx_exe_image.h"
#include "runtime_composition.h"
#include "title_identity.h"

#include <filesystem>
#include <iostream>
#include <lucent/log.h>
#include <memory>
#include <string>

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
  const std::string executable = ctr::kTitleIdentity.defaultExecutable(ctr::kProvisioningRoot).string();
  if (!std::filesystem::is_regular_file(executable)) {
    lucent::error("boot", "{} is absent; provision the verified CTR USA executable first", executable);
    return 2;
  }

  ctr::CtrRuntime runtime(ctr::native::kExecutableEntry);
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  Core *core = &game->core;
  watchdog_init();
  load_exe(executable.c_str(), core);
  if (!core->lightrecExecutor().available()) {
    lucent::error("boot", "psxport was built without its Lightrec dynarec backend");
    return 2;
  }

  psx::Machine machine(*game);
  machine.bindDevices();
  ctr::installRuntimeOwners(*game);
  machine.prepare();

  lucent::info("boot", "entering CTR at measured executable entry 0x{:08X}", ctr::native::kExecutableEntry);
  machine.attachControlChannel();
  machine.run();
  return 0;
}
