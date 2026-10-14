---
id: 38
title: Race widescreen owners are parked, not landed
status: open
tags: widescreen,race,hud
created: 2026-10-14
---

## State
An unfinished race widescreen pass (view frustum owner, HUD anchor owner, projection distance scale,
frame driver hooks) was parked uncommitted so the boot restructure could land. It failed
`ctr_widescreen_owner` ("H is scaled… got 8192, expected 10944") because `projection_owner.cpp` set the
distance scale to 1/1.

## Where it is
`scratch/parked-race-widescreen/` (gitignored, kept by `.scratch-keep`): `tracked.diff` against
`cf6c4a2` plus the four new `game/video/*_owner.*` files.

## Next
Re-apply on the current tree, decide the distance scale from the guest projection, then gate.
