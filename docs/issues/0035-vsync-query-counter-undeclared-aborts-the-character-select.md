# Character select aborts: VSync(-1) has no declared libetc counter

Fixed 2026-10-09. Observed: Arcade, Single, 1P, Easy, Select Character, Cross aborted the product with
`[plat-hle:error] VSync negative query at 0x80075350 has no measured libetc field counter`. Expected: the race loads.

Reproduction: `tap` Start at the title, Down twice, Cross four times (Arcade, Single, 1P, Easy), Cross on the character
select, over the control channel on a fresh card; the abort follows within a field.

Decompiled path: `FUN_80075350` (libetc `VSync`) returns `DAT_8008c754` for a negative mode and otherwise waits;
`FUN_8007c8d8` (the libetc vblank handler, called from the ISR chain) increments `0x8008C754` once per vblank.

Cause: `game/execution/platform_hle_plan.cpp: kPlatformHlePlan` declared `vsyncAddress` but no `vsyncQueryCounterAddress`, so
psxport's `PlatformHle::vsync` had nothing to answer VSync(-1) from; the character-select code is the first caller.

Fix at the owner: `native::kVSyncQueryCounter = 0x8008C754` in `game/title/native_ownership.h`, declared in the plan.

Test: `tests/ctr_platform_hle_plan.cpp` (`ctr_platform_hle_plan`) binds the shipping plan and asks the bound VSync for -1;
it aborted before the fix and returns the counter word after.
