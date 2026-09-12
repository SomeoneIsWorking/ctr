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
publication are now wired; nested-exit proof, runtime denominators, and the later saved frontier
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

The remaining smallest runtime discriminator is bounded counts at `0x8006A610`, `0x8006A57C`,
and `0x8006A6B0`, plus `t9` and descriptor values across one continued budget slice. Advancing
`t9` and a later frame exit would identify a finite quantum; a repeated descriptor/pointer without
expected progress would redirect investigation to the render list or guest control flow. Neither
explanation is established by one exit sample. The corrected run reaches this budget path; its
descriptor and pointer progression now need measurement before changing the budget or render path.
Resolve both exits before reaching issue 0023's current boundary with the existing native owners
and nonzero Lightrec execution. Representative interactive gameplay, independent state/device comparison,
override/original-call coverage, invalidation controls, product link/selector proof, and released-
host qualification remain required. The deleted static machinery must not return.
