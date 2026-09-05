---
id: 19
title: CTR load pipeline stalled forever on an undelivered libcd read-completion callback
status: resolved
symptom: Every presented field was black; the screen loader at 0x80033610 returned stage 2 unchanged for 161226 consecutive host fields
state_items: S003,S005
tags: cd,libcd,callback,loader,black-screen,frame-loop
created: 2026-08-28
updated: 2026-08-28
---

## Symptom, measured

A headless product run reached sustained presentation and stayed alive, but `PSXPORT_PRESENT_SHOT_AT`
and `PSXPORT_SHOT_AT` both captured 0.00% non-black pixels (present 960x720 and guest VRAM 512x216 at
alternating display Y 0/296), and `PSXPORT_GP0RAW`/`PSXPORT_PRIMDUMP` showed 40 GP0 command words and
2 degenerate white quads per frame — an empty ordering table, not a rendering fault.

`PSXPORT_WWATCH=8008D0F4,8008D0FC` (retail `gp`=0x8008CF6C, so `gp+0x188` state and `gp+0x18C`
request) recorded main entering state 3 once, then `0x8003CC98` storing the screen-loader result
1, then 2, then 2 for 161226 further fields with no other value ever stored.

## Root cause

`FUN_80033610` (the state-3 screen loader) returns its stage unchanged while the load-busy flag at
`gp+0x138` is non-zero. Stage 1 calls `FUN_800334F4`, which sets that flag and queues an async load;
the pump `FUN_80032DC0` dispatches it through `FUN_800321B4`, which registers a libcd read-completion
callback (`CdReadCallback` 0x800771B0 -> slot 0x8008AD10 = `FUN_80032110`) and then returns without
polling, because retail signals completion from the CD interrupt.

psxport's native `cd_read_stock_sync` transfers every requested sector before the CdRead leaf returns
and delivers no callback, so that slot is never written again. `PSXPORT_WWATCH=8008AD10,8008AD14`
proved it exactly: 0x8003254C was registered and cleared four times (the old hand-published owner),
and 0x80032110 was registered once at field 2 and never cleared for the rest of the run. The flag
therefore stayed 1, the loader never left stage 2, and nothing was ever submitted to draw.

## Fix

`game/core/async_disc_owner.{h,cpp}` now owns the measured stock libcd CdRead leaf: it runs the shared
`cd_read_stock_sync` transfer and then dispatches whatever callback the retail slot holds with libcd's
`CdlComplete` (2), restoring the interrupted register context as a real CD interrupt entry would. It
transcribes none of the callback's effects — the retail body owns them.

That supersedes the previous `AsyncDiscOwner`, which hand-published `FUN_8003254C`'s two effects
around a preserved `0x80032594` super. Dispatching the real callback covers that path too, so the
override, its preserved super, and the transcription constants are removed rather than annotated.

## Evidence

A live identity-verified real-disc run advanced the loader 2 -> 3 -> 4 -> 5 across fields 1-4 instead
of stalling, and the delivery log reported one polled read and one delivered callback. The asset-free
`ctr_runtime_test` covers both branches: the no-callback read is counted and reported, and a delivered
callback is dispatched with a0 = 2, clears the slot from the callback body, and leaves v0/ra exactly
as the interrupted caller left them. `verify` and CTest 8/8 pass on Clang.

## Exact remaining frontier

Execution then failed fast one stage later at unmapped executable address `0x800B0B38` from caller
`0x800368BC`. That address is above the resident `0x8008D800` text extent, establishing that the
game had loaded and called a runtime module. The module identity facts are retained in C020/I016;
activation and invalidation through Lightrec remain part of issue 0024.
