#include "async_disc_owner.h"
#include "core.h"
#include "ctr_runtime.h"
#include "game.h"
#include "game_runtime.h"
#include "native_ownership.h"

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
  ctr::CtrRuntime runtime(0x80010000u);
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  auto otherGame = std::make_unique<Game>();

  auto &completion = ctr::discReadOwner(game->core);
  auto &otherCompletion = ctr::discReadOwner(otherGame->core);
  check(&completion != &otherCompletion, "two Cores share one pending CD completion owner");
  game->core.mem_w32(ctr::native::kCdReadCompletionCallback, 0x80032110u);
  completion.noteTransferComplete(game->core);
  check(completion.hasPendingCompletion(), "registered callback did not become owed");
  check(!otherCompletion.hasPendingCompletion(), "another Core inherited the first Core's callback");
  otherCompletion.noteTransferComplete(otherGame->core);
  check(otherCompletion.polledReads() == 1u, "second Core's polled read was not counted separately");
  check(completion.polledReads() == 0u, "second Core's polled read changed the first Core's count");
  game->core.mem_w32(ctr::native::kCdReadCompletionCallback, 0u);
  completion.deliverPending(game->core, runtime);
  check(!completion.hasPendingCompletion(), "cancelled callback remained owed to the first Core");
  check(otherCompletion.polledReads() == 1u, "first Core's cancellation changed the second Core's count");

  std::printf("CTR per-Core disc completion: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
