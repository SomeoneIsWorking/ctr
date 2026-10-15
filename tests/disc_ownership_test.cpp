// Disc read ownership.

#include "core.h"
#include "disc/async_disc_owner.h"
#include "game.h"
#include "game_runtime.h"
#include "runtime/runtime.h"
#include "runtime/session.h"
#include "title/native_ownership.h"

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
  auto otherGame = std::make_unique<Game>(runtime, psx::config::Config{});

  auto &completion = ctr::session(game->core).discRead;
  auto &otherCompletion = ctr::session(otherGame->core).discRead;
  check(&completion != &otherCompletion, "two Cores share one pending CD completion owner");
  game->core.mem_w32(ctr::native::kCdReadCompletionCallback, 0x80032110u);
  completion.noteTransferComplete(game->core);
  check(completion.hasPendingCompletion(), "registered callback did not become owed");
  check(!otherCompletion.hasPendingCompletion(), "another Core inherited the first Core's callback");
  otherCompletion.noteTransferComplete(otherGame->core);
  check(otherCompletion.polledReads() == 1u, "second Core's polled read was not counted separately");
  check(completion.polledReads() == 0u, "second Core's polled read changed the first Core's count");
  game->core.mem_w32(ctr::native::kCdReadCompletionCallback, 0u);
  completion.deliverPending(game->core);
  check(!completion.hasPendingCompletion(), "cancelled callback remained owed to the first Core");
  check(otherCompletion.polledReads() == 1u, "first Core's cancellation changed the second Core's count");

  std::printf("CTR per-Core disc completion: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
