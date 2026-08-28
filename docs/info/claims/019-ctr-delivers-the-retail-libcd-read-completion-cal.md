---
id: C019
kind: claim
status: holds
created: 2026-08-28
tags: ctr04,ctr08,cd,libcd,callback,loader
depends: game/core/async_disc_owner.cpp#deliverCompletion, game/core/async_disc_owner.cpp#cdReadWithCompletionCallback, game/core/platform_hle_plan.cpp#kPlatformHlePlan, game/core/native_ownership.h#kCdReadCompletionCallback
---

## Claim

CTR delivers the retail libcd read-completion callback after the native synchronous CdRead, and that delivery is what advances the state-3 screen loader past its stalled stage

## Evidence

On identity-verified SCUS_944.26, `PSXPORT_WWATCH=8008AD10,8008AD14` recorded callback 0x80032110
registered once and never cleared for the whole run, while `PSXPORT_WWATCH=8008D0F4,8008D0FC` recorded
the loader result stored as 2 for 161226 consecutive fields and every present captured 0.00% non-black
pixels. With the owner in place, the same real-disc run stored 2, 3, 4, 5 across fields 1-4 and the
owner logged one polled read plus one delivered callback. The asset-free `ctr_runtime_test` requires
both branches: a read with an empty slot is counted and reported, and a registered callback is
dispatched exactly once with a0 = `CdlComplete`, clears its own slot from the retail body, and leaves
v0 and ra byte-identical to the interrupted caller. `verify` and CTest 8/8 pass on Clang against
framework ff21584d.

## What would falsify it

The measured slot 0x8008AD10, its writer `CdReadCallback` 0x800771B0, the CdRead leaf 0x80076F10, or
the CdlComplete status changes; a run shows the callback dispatched with the caller's context altered
or dispatched more than once per read; or the loader advances past stage 2 with the delivery removed
