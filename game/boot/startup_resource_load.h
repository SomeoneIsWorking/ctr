#pragma once

#include <cstdint>

class Core;

namespace ctr {

class CtrRuntime;
class FieldBoundary;

// State zero's startup resource load, with the one guest VSync inside it removed.
//
// `0x80031FDC` is reached in two ways and its fifth argument selects the branch: any other value runs
// ordinary retail code, and -1 selects a path that performs its setup and then calls VSync(2). The
// title owns the timing, so it may not make that call, but everything the call follows is the game's
// — the resource setup at 0x800321B4, the commit at 0x80031EE4, and the exact caller frame both read
// from. This owner transcribes that frame, keeps the retail helpers executing through Lightrec, waits
// the field the call owed, and resumes at the post-call instruction.
//
// It also owns the "am I mid-load" question: `ownsSuffix()` and `consumeWaitedField()` are the only
// two decisions the load makes, and so the only two a test needs to reach.
class StartupResourceLoad final {
public:
  StartupResourceLoad(CtrRuntime &runtime, FieldBoundary &field);

  // True while this owner holds a suffix to run. A fresh owner holds none.
  [[nodiscard]] bool ownsSuffix() const;

  // Counts down one of the fields the omitted VSync owed the guest. Returns true when a field was
  // owed and has now been served, which is the caller's cue to end the field. A field that is not
  // owed returns false and changes nothing, so the countdown cannot be driven below zero.
  [[nodiscard]] bool consumeWaitedField();

  // Serves the fifth-argument -1 branch. Returns without owning anything for any other argument,
  // after running retail's own body.
  void begin(Core &core);

  // Runs the suffix the waited field owed, restores the measured caller, and hands the field back to
  // the guest.
  void resume(Core &core);

private:
  CtrRuntime &runtime_;
  FieldBoundary &field_;
  uint32_t suffixAddress_ = 0;
  uint32_t waitedFields_ = 0;
};

} // namespace ctr
