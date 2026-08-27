#include "command_line.h"

#include <ostream>
#include <string_view>

namespace ctr {

CommandLineAction parseCommandLine(int argc, char *const argv[]) {
  if (argc == 1) {
    return CommandLineAction::run;
  }
  if (argc == 2 && argv[1] != nullptr) {
    const std::string_view argument(argv[1]);
    if (argument == "-h" || argument == "--help") {
      return CommandLineAction::help;
    }
  }
  return CommandLineAction::invalid;
}

void printUsage(std::ostream &output, const char *program) {
  output << "Usage: " << program << " [--help]\n"
         << "Run the verified CTR USA port using the configured disc.\n";
}

} // namespace ctr
