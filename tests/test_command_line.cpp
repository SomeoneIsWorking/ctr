#include "command_line.h"

#include <cassert>
#include <sstream>
#include <string>

int main() {
  char program[] = "ctr_port";
  char shortHelp[] = "-h";
  char longHelp[] = "--help";
  char invalid[] = "disc.bin";

  char *runArguments[] = {program};
  assert(ctr::parseCommandLine(1, runArguments) == ctr::CommandLineAction::run);

  char *shortHelpArguments[] = {program, shortHelp};
  assert(ctr::parseCommandLine(2, shortHelpArguments) == ctr::CommandLineAction::help);

  char *longHelpArguments[] = {program, longHelp};
  assert(ctr::parseCommandLine(2, longHelpArguments) == ctr::CommandLineAction::help);

  char *invalidArguments[] = {program, invalid};
  assert(ctr::parseCommandLine(2, invalidArguments) == ctr::CommandLineAction::invalid);

  std::ostringstream usage;
  ctr::printUsage(usage, program);
  assert(usage.str() == "Usage: ctr_port [--help]\nRun the verified CTR USA port using the configured disc.\n");
}
