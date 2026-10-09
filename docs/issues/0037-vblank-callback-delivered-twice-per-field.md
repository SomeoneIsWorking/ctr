# The retail VSync callback runs twice per field

Open. Observed in a race: `gGT+0x1CE4` (incremented by the registered vblank callback `FUN_80034AA4`) rises by 2 per host
field while libetc's own vblank counter `0x8008C754` rises by 1. Expected 1 and 1: the retail frame waits for two callbacks
(`[gp+0x348]` starts at 2, the callback decrements it), i.e. two fields per game frame (30 fps, dt 32).

Result: one game frame per field (`gGT+0x1CEC` +1 per field), dt 16 after issue 0036. HUD time is correct but the simulation steps
at 60 Hz with half the retail step.

Cause: two deliveries. The framework raises the vblank IRQ and the guest's libetc ISR `FUN_8007C8D8` calls the registered
callback; `game/frame/frame_callback_owner.cpp: FrameCallbackOwner::deliverField` also calls it directly every field.

Tried: removing the direct delivery stalls boot after BF0225 is published (the early boot waits need the callback before the
guest's IRQ path delivers one), so the direct call carries boot; whether it is redundant after some boot point is not yet shown.

Proper fix to investigate: find why the ISR does not deliver the callback during early boot (I_MASK vblank enable / libetc init) and
retire the direct call once it does; then re-measure dt (expected 32 per two fields), and re-record `replays/gameplay`.
