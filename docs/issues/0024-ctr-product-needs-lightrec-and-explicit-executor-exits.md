---
id: 24
title: CTR product needs Lightrec and explicit executor exits
status: open
symptom: Static execution and FrameCompleted unwinding are removed, but the replacement Lightrec path is not yet verified on the preserved frontier
state_items: S009,S010
tags: ctr,dynarec,lightrec,executor,frame-loop,migration
created: 2026-09-04
updated: 2026-09-12
---

## Root cause

CTR was composed around a static dispatcher and local `FrameCompleted` exception unwinding. The
break-first migration removed both. The per-Core Lightrec path and exact resident/BIGFILE image
publication are now wired; complete nested-exit proof, sustained runtime denominators, and the later saved frontier
remain incomplete.

## Required resolution

Consume psxport's per-`Core` Lightrec executor and map authenticated resident/overlay images directly.
Register native overrides by image generation plus guest address; original calls bypass only the
active override and execute through Lightrec; overlay replacement invalidates affected blocks.

Each former `FrameCompleted` site records its exact title continuation/phase and requests a typed
executor exit. Lightrec must return normally at a safe dispatcher boundary with synchronized
state. The frame driver validates the result, lets scoped driver/override ownership unwind normally,
finishes exactly one field/presentation fence, and increments its counter. Unknown exits,
unexpected normal return, and stale continuation state remain fatal.

The first live image discriminator crossed `0x800B0B38` after publishing the three measured BIGFILE
images, then stopped on a `budget-exhausted` exit at resident `0x8006A57C` in frame 12,771. The
saved log records only the exit PC, not its guest registers, loop count, or budget detail. The exact
SHA-256-verified USA executable places that PC inside the hand-written GTE primitive path entered at
`0x8006A52C`, the interior entry already identified in issue 0022. At `0x8006A610`, this path reads
descriptor `t3` from `t9`, advances `t9` by four bytes, and branches to `0x8006A52C` for a negative
descriptor. Three indirect calls and GTE vertex loads precede `0x8006A57C`, which masks the
descriptor's low nine bits. `0x8006A6B0` branches back to `0x8006A57C` during the primitive path;
an all-ones descriptor reaches the exit at `0x8006AD20` via `0x8006A52C`. These are binary control-
flow facts, not evidence that this live list is malformed or that its loop is infinite.

At the recorded framework revision, `ExecutionBudget::currentTurn` allowed 564,480 guest cycles
per dispatch and the CTR frame driver treated any budget exit as fatal. A single bounded GDB run on
the Clang/Lightrec product hit the cycle-budget return once, after the same three published images
and callbacks 13–18: `nextPc` and synchronized `Core::pc` were both `0x8006A57C`, and the executor
had consumed 564,492 cycles against the 564,480-cycle allowance. It was not the separate host-
dispatch-budget exit. Guest `t3=0x1C123824` is nonnegative with low nine bits `0x024`,
`t9=0x80129C4C` lies 0x99EC bytes inside the latest 328-sector read at `0x80120260`,
`s6=0x8006A8E0`, `sp=0x8012A2AC`, and `ra=0x8006A69C` is the return after `jalr s6` at
`0x8006A694`. The run stopped at its first budget breakpoint and did not sample successive
descriptors or continue another budget slice.

An opt-in GDB probe against the same shipping Clang product was first checked on the asset-free
Lightrec frame test: 6 block-entry callbacks included 2 hits at the selected positive resume PC
and 0 at an impossible negative PC. It labels its observations as block entries, not exact guest
instruction counts. Two bounded CTR disc runs then failed before that probe's budget trigger. The
first omitted `PSXPORT_ASSET_DIR` and enabled `PSXPORT_NOPACE`, so its missing menu assets and altered
pacing make it unsuitable for comparing the recorded frontier; it stopped at frame 6,422 with a
Lightrec `Fault` at `0x800ABDE0` after 181,084 cycles. That first probe mistakenly executed one
additional slice from the fault PC; it returned the same `Fault` after 2 cycles and did not reach
the budget path. The corrected headless, silent run supplied
the framework asset directory and retained normal pacing; it loaded 4/4 Rml assets, published
BF0225/BF0226/BF0233, delivered callbacks 13–18, then stopped at frame 10,856 with the same
`Fault`/PC and 181,084 cycles. `0x800ABDE0` lies in the published BF0233 image. Neither run
reached `0x8006A57C`, so the retail block-entry probe was not armed, the three target counts were
**not measured**, and no extra budget slice was executed. The corrected probe did not continue after
the distinct fault. The fault's exact Lightrec exit flag remains unknown. These negative runs do not erase the earlier reached budget observation or
establish why the path diverged; both used the existing Clang binary built before that observation.

A subsequent source and authenticated-image discriminator identified a title-foreign guest write at
this exact PC. The extracted BF0233 image has 56,844 bytes and FNV64
`0xA4E9995DC4FD53F7`, both matching the committed descriptor. It begins at `0x800AB9F0`; byte offset `0x3F0`
contains instruction word `0xAFB3002C` (`sw s3,0x2c(sp)`) between the preceding stack prologue
and following saved-register stores. `CtrFrameDriver::stepFrame` calls shared
`Timing::frameTick()` once per field. That shared method also stores its host VBlank count to
`0x800ABDE0`, an address recovered for **Tomba! 2** rather than CTR. The BF0233 callback publishes
the image after the field's tick; the next field overwrites its executable prologue word. This is a
proven cross-title RAM collision and a direct mechanism for the later Lightrec fault at the same PC.
The exact Lightrec flag and live overwritten word were not captured, so the fault subtype is still
an inference. A new asset-free CTR translated-frame regression installs that BF0233 range and word,
then runs the shipping field owner; against the old framework it fails on all 3/3 field ticks, with
17 translated blocks, 71 translated instructions, and zero interpreter fallback. The same focused
Clang test passes after the shared Timing change keeps only the host count. Tomba! 2 now mirrors
that count at its measured title frame boundary; its focused two-writer/per-Core regression passes.
The framework and both title changes passed their combined Clang gates; CTR's framework pin records
the corrected shared revision.

A corrected, uninstrumented CTR retail run with the Clang product, framework assets, normal pacing,
headless presentation, and silent audio published BF0233 generation 4, delivered callbacks 15–18,
and left frame 12,051 at `0x8006A57C` with `budget-exhausted`. It did not reproduce the previous
`0x800ABDE0` fault. A separate bounded GDB run stopped when the shipping BF0233 publisher ran and
at the next field's `Timing::frameTick()` boundary: the authenticated three-word prologue
`0xAFB40030 0xAFB3002C 0xAFB20028` was unchanged before and after that tick (field 13,644).
These observations establish the corrected live word's survival and expose the prior render-list
budget frontier again. They do not establish interactive gameplay or complete budget behavior.

`tools/ctr_budget_probe.py` now provides an opt-in Clang/GDB discriminator for that exact exit. It
refuses any exit other than `BudgetExhausted` at synchronized `Core::pc=0x8006A57C`, requires exactly
one active BF0233 catalogue entry spanning its measured image range and the unchanged prologue word,
then calls the ordinary `CtrRuntime::dispatch` once from that PC with the unchanged current-turn
budget. It records before/after `t9` and `t3`, the second typed result and cycle count, full
translated block/instruction deltas, and target-PC hits at Lightrec block entries. It observes at
most 4,096 block entries because GDB traps are slow, labels counts after that cap incomplete, and
reports both scanned and retained counts including zero matches. Its Linux Clang synthetic controls
drive the same executor with an advancing pair, an unchanged pair, an absent BF image, and an absent
GDB trigger; 4/4 pass.
The bounded diagnostic has been run once against retail CTR with the singleton headless game slot;
the tool forces headless/silent presentation and normal pacing. It disables only
the host frame-progress watchdog (`PSXPORT_WATCHDOG=0`) while GDB pauses the guest at block entries;
the guest spin detector remains active. An external five-minute timeout allows the ~12,000 normally
paced fields to reach the trigger and stops only the spawned GDB process group. GDB success without
one admitted trigger is a failure, and an admitted trigger executes
exactly one continued slice. The synthetic gate is
`uv run --frozen python tools/ctr_budget_probe.py --selftest`.

The retail probe admitted `BudgetExhausted` at synchronized `0x8006A57C` with one active BF0233
match among four scanned images. One unchanged `CtrRuntime::dispatch` then reached typed
`FrameBoundary` at `0x8003CEB4` after 110,766 cycles, 5,215 translated block executions, and
56,904 translated instructions. GDB observed the first 4,096 of those block entries; the remaining
1,119 were not inspected. Within that observed prefix `t9` moved from `0x80129C4C` through
`0x8012A24C`, with 282 entries at `0x8006A57C`, 316 at `0x8006A610`, zero observed at
`0x8006A6B0`, and zero at the impossible negative PC. The zero at `0x8006A6B0` is limited to the
observed prefix and to block entries, not all executed guest instructions. This establishes a finite
render-list quantum, not an infinite guest loop. The immediate title cause was
`CtrFrameDriver::stepFrame` treating every first-turn budget exit as fatal despite the executor's
synchronized continuation and the subsequent valid frame boundary.

The frame driver now continues positive-cycle, synchronized `BudgetExhausted` exits through the
ordinary shipping `CtrRuntime::dispatch` inside the same field. It accepts only a typed
`FrameBoundary` to finish that field; other exits keep the existing fatal path. It rejects a
zero-cycle or desynchronized budget exit instead of retrying a non-progressing host loop. There is
no guessed per-field turn ceiling: the existing guest spin detector and host frame-progress watchdog
remain the runaway guards. The spin detector covers host-starved in-region loops, while the default
frame watchdog bounds elapsed time without presentation; disabling both removes that protection.
An asset-free production-driver fixture completed one field after six
budget exits with 564,483 translated block executions, 1,693,451 translated instructions, zero
fallback blocks, one timing tick, and one presentation fence. Its negative forces an image-scoped
native return to the same PC until the host-dispatch budget exits with zero guest cycles; the driver
rejects that exit. An authenticated, headless, silent retail run after the change crossed the former
budget stop and continued through BF0233 publication and read callbacks 15–18. It next aborted in
frame 16,228 with a typed `Fault` at guest PC `0x8006AA80`; Lightrec reported invalid load/store
addresses `0x1F800938` and `0x1F80093C`. The run made no visible-gameplay claim and did **not**
reach issue 0023's preserved `0x8006AB00` fault. Its raw log is gitignored at
`scratch/logs/ctr-frame-continuation-retail.log`.

One bounded, authenticated GDB run then captured both invalid Lightrec map accesses once each.
Their opcode words match resident `lw t0,0x140(a1)` at `0x8006A8F4` and its
`lw v1,0x144(a1)` delay slot at `0x8006A8FC`. At both accesses, the live source
registers were `t3=0xFFFFFFFF`, `at=0x1F800000`, and `a1=0x1F8007F8`, yielding
`0x1F800938` and `0x1F80093C` beyond the 1 KiB scratchpad. The run had one active
BF0233 entry at its authenticated range among four images scanned. The list pointer
`t9=0x801B6074` was in main RAM; its preceding word was `0xFFFFFFFF`, but the
producer and validity of that sentinel have not been established. This run stopped
in frame 16,315 with typed `Fault` at `0x8006C0FC` after 40,480 guest cycles;
the different typed PC from the earlier frame 16,228 run does not alter the two
measured effective addresses. The fixed read-only trace is gitignored at
`scratch/logs/ctr-bad-map-retail.log`.

The next discriminator is the authenticated producer of `t3=0xFFFFFFFF` and the
render-list invariant that should accept or reject it, before attributing the fault to
issue 0023's later `0x8006AB00` input. Nested original-call continuations have separate
scoped-PC ownership and have not been qualified for budget continuation by this top-level field
test. Representative interactive gameplay, independent state/device comparison,
override/original-call coverage, invalidation controls, product link/selector proof, and released-
host qualification remain required. The deleted static machinery must not return.
