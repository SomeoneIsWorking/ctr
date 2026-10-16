#include "runtime/runtime.h"

#include "core.h"
#include "execution/platform_leaves.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "runtime/session.h"
#include "title/native_ownership.h"

#include <lucent/log.h>

#include <cstdlib>
#include <cstring>

namespace ctr {
namespace {

// CdRead is the title's wrapper (the owed completion callback) and CdSync answers as CdInit expects.
constexpr psx::cd::StockLibcd::Entries kLibcdEntries{.readSync = native::kCdReadSync, .command = native::kCdCommand};
// libcd's CdlDataReady, from the callback's own bytes; a synchronous CdRead owes no INT1 completion.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdReadyCompletion{.status = native::kCdReadyCompletionStatus,
                                                                      .perStockRead = false};

} // namespace

Runtime::Runtime()
    : vsync_({.entry = native::kVSync, .queryCounter = native::kVSyncQueryCounter}),
      libgte_({.setGeomOffset = native::kSetGeomOffset, .setGeomScreen = native::kSetGeomScreen}),
      libcd_(kLibcdEntries, {}, native::kCdReadyCallbackSlot, kCdReadyCompletion) {}

const Runtime &Runtime::from(const Core &core) {
  const auto *const runtime = dynamic_cast<const Runtime *>(&core.runtime);
  if (runtime == nullptr) {
    lucent::error("ctr-runtime", "a CTR owner ran on a core with no CTR runtime");
    std::abort();
  }
  return *runtime;
}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libgte_.bind(game);
  libcd_.bind(game);
  bindPlatformLeaves(game);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>();
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(game);
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  return session(core).sceneCut.isCut();
}

const GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &widescreen_;
}

bool Runtime::titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  if (std::strcmp(input.command, "warp") != 0) {
    return false;
  }
  auto *driver = dynamic_cast<FrameDriver *>(core.game->frameDriver.get());
  std::fprintf(reply,
               "%s\n",
               driver == nullptr ? "refused: no CTR frame driver is running"
                                 : driver->armWarp(core, input.line).c_str());
  return true;
}

} // namespace ctr
