# 0028 — the frame-timing bridge dereferences the game-state descriptor it never checks, and the
# function below it checks the very same pointer

Found while extracting the field owners out of `game/core/frame_driver.cpp`. **Not fixed here** — the
extraction was behaviour-preserving by instruction, and this is a pre-existing latent hazard, not a
regression.

## The two functions, side by side

`FrameSuffix::completeFieldTiming` (`game/core/frame_suffix.cpp`):

    core.r[3] = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
    core.mem_w32(core.r[20] + kUnattributedWordOffsetFromS4, core.r[2]);
    core.r[2] = core.mem_r32(core.r[3] + kDebugVSyncFlagWordOffset);   // <- r3 never checked
    core.r[2] &= kDebugVSyncFlagMask;

`FrameSuffix::readWaitState`, twenty lines below it in the same file:

    wait.gameState = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
    if (wait.gameState == 0u) {
      return wait;                                                     // <- the same pointer, checked
    }

So one function trusts the descriptor and the next validates it, in the same translation unit, for
the same `gp`-relative word. The comment above the unchecked read already says the value is a
"debug-flag read" that decides whether the conditional `VSync(0)` is made at all.

## Why the unchecked form is worse than it looks

With `r3 == 0` the read does not fault. `kDebugVSyncFlagWordOffset` is `9580` = `0x2576`, and
`0x00000000 + 0x2576` is ordinary PSX RAM, not unmapped space. The result is therefore a
`debugVSync` decision taken from an unrelated RAM word, which then sets or does not set `ra` and
`a0` for the frame suffix and changes the field's accounted instruction count between 7 and 9. A
refusal would have been loud and wrong-but-safe; this is quiet and plausible.

## Why it is not currently reachable, and why that is not a reason to leave it

The recorded runs enter state 3, and state 3 publishes the game-state descriptor before the
frame-timing owner is first reached, so `r3` is non-zero on every recorded path. But:

1. The code that reads it is the code that runs **before** the field's first complete frame, so the
   ordering that makes it safe is a property of the boot sequence rather than a stated precondition.
   A new entry into the frame loop — an attract restart, a scene reload, the issue-0023 frontier
   being crossed after a fault — can reach the owner without it.
2. The fix is one line in the same file and one assertion, and the predicate immediately below shows
   exactly what "absent" means.

## What a fix would be

`completeFieldTiming` should read the descriptor through the same named accessor `readWaitState`
uses, and refuse — or take the documented absent-descriptor answer — when it is zero, so the two
functions cannot disagree about whether the descriptor exists. That is a behaviour change on a path
no recorded run exercises, which is precisely why it should be a deliberate, separately-tested change
and not a side effect of moving code between files.
