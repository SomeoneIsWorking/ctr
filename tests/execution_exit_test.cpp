// Typed executor exit requests.

#include "core.h"
#include "execution/guest_turns.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "game.h"
#include "game_runtime.h"
#include "runtime/runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

} // namespace

int main() {
  ctr::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});

  const psx::cpu::ExecutionResult expected{
      psx::cpu::ExecutionExitReason::FrameBoundary, 0x8003CEB4u, 73u, "nested retail frame boundary"};
  ctr::guest::propagateFrameBoundary(game->core, expected, "CTR execution-exit test");
  const auto actual = game->core.executionControl().consume();

  check(actual.has_value(), "frame boundary was not published to the Core execution control");
  if (actual) {
    check(actual->reason == expected.reason, "frame boundary reason changed during propagation");
    check(actual->guestPc == expected.guestPc, "frame boundary guest PC changed during propagation");
    check(actual->cycles == expected.cycles, "frame boundary cycle count changed during propagation");
    check(actual->detail == expected.detail, "frame boundary detail changed during propagation");
  }

  std::printf("CTR typed execution exit: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
