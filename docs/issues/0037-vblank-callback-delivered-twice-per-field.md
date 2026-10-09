# The retail VSync callback runs twice per field

Fixed 2026-10-09. Observed in a race: `gGT+0x1CE4` (incremented by the registered vblank callback `FUN_80034AA4`) rose by 2 per
host field while libetc's counter `0x8008C754` rose by 1. Expected 1 and 1: `[gp+0x348]` starts at 2 and the callback decrements
it, so a game frame is two fields (30 fps, dt 32).

Decompiled path: `FUN_80077254` registers the callback into libetc's chain; `FUN_8007C8D8` (the vblank ISR handler) increments
`0x8008C754` and calls the eight chain slots at `0x8008C734`.

Two deliveries: the framework's `Hle::irqPoll` ran the ISR at guest function entries, and `FrameCallbackOwner::deliverField` also
called the callback directly every field.

Why the direct call looked necessary: with it removed, boot stalled after BF0225. The frame suffix wait
(`FrameSuffix::resume`, `[gp+0x348]` still above zero) ends the field with no guest code running, and `irqPoll` runs only at guest
function entries, so the latched vblank edge was never delivered and the countdown never drained.

Cause: `game/frame/frame_callback_owner.cpp: FrameCallbackOwner::deliverField` replaced the guest's interrupt delivery with its own
callback call instead of polling the interrupt at the seam where no guest code runs.

Fix at the owner: `deliverField` polls `Hle::irqPoll` when the IRQ gate is armed, so the guest's own ISR runs the callback once per
edge; the direct callback, its registration override (`onVblankCallback`) and `kVblankCallbackInstall` are deleted.

Test: `tests/ctr_frame_callback_owner.cpp` (`ctr_frame_callback_owner`) latches a vblank with an ISR element on the chain; before the
fix the element was never called, after it is called once per edge and not again once acknowledged.

After: `gGT+0x1CE4` and `0x8008C754` both rise 1 per field; `gGT+0x1CEC` (game frames) rises 1 per two fields; dt is 33-34 (retail
nominal 32; the measured elapsed includes guest instruction time past the field boundary); the race clock advances about 16.5 per
field, so HUD time matches wall time.
