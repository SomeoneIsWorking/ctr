#pragma once

#include "disc/async_disc_owner.h"
#include "game_runtime.h"
#include "video/scene_cut.h"

class Core;

namespace ctr {

// What one Core of CTR owns outside guest RAM, created with its Game.
class Session final : public psx::game::TitleSessionState {
public:
  // A new session's first field is a cut.
  SceneCut sceneCut;
  // The CD read completion the title's CdRead owes the guest.
  DiscReadOwner discRead;
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace ctr
