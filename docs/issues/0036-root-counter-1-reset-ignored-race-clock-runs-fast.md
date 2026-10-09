# Race clock runs 4x: the guest's reset of root counter 1 was swallowed

Fixed 2026-10-09 (psxport). Observed: in a race the HUD timer advanced 64 units (1/960 s) per host field, 14 s of HUD time
per 213 fields (3.5 s). Expected 16 per field (HUD time equals wall time at 60 fields/s).

Reproduction: Arcade/Crash/Crash Cove, pause in the race, `step 1`, read `gGT+0x1D04` (the frame's dt) and `gGT+0x1D10`
(race timer) with `rw`; dt was 64 every step.

Decompiled path: `FUN_80034D54` (gameplay update) takes `elapsed = FUN_8004B41C(prev)`, `dt = elapsed*32/100` (32 if negative, capped at 64);
`FUN_8004B3A4` returns `((gp[0xA1C] + GetRCnt(1)) * 1000) / 5246`; the vblank callback `FUN_80034AA4` does
`gp[0xA1C] += GetRCnt(1)` and then `ResetRCnt(1)` (`FUN_80077C80`, a halfword store of 0 to `0x1F801110`).

Cause: `psxport/runtime/psx/platform/io_peripherals.cpp: io_peripheral_write` dropped writes to root counter 1, so
`Timing::hSyncCounter` stayed an absolute HBlank count and `gp[0xA1C]` grew by the whole count every vblank (about 108,000 per
step); `elapsed` was always huge and dt sat at its 64 cap.

Fix at the owner: `Timing::hSyncCounterWrite` keeps an offset (`rootCounter1Offset`), `hSyncCounter` returns the count since
the last write; the offset is part of the device-bus timing section (layout version 2).

Test: `psxport/tests/test_hsync_counter.cpp: root_counter_one_counts_from_a_guest_write` (failed with 789 where 0 was written).

After: dt is 16 per field.

Open: issue 0037.
