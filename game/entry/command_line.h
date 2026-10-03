#pragma once

#include <iosfwd>

namespace ctr {

enum class CommandLineAction {
  run,
  help,
  invalid,
};

CommandLineAction parseCommandLine(int argc, char *const argv[]);
void printUsage(std::ostream &output, const char *program);

} // namespace ctr
