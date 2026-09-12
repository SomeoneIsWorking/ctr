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
the distinct fault. The fault's exact Lightrec exit flag and
cause remain unknown. These negative runs do not erase the earlier reached budget observation or
establish why the path diverged; both used the existing Clang binary built before that observation.

The remaining smallest runtime discriminator is bounded counts at `0x8006A610`, `0x8006A57C`,
and `0x8006A6B0`, plus `t9` and descriptor values across one continued budget slice. Advancing
`t9` and a later frame exit would identify a finite quantum; a repeated descriptor/pointer without
expected progress would redirect investigation to the render list or guest control flow. Neither
explanation is established by one exit sample. First classify the newly reached BF0233 fault's
Lightrec exit flag and inputs, then repeat the budget probe only if that path reaches its trigger.
Resolve both exits before reaching issue 0023's current boundary with the existing native owners
and nonzero Lightrec execution. Representative interactive gameplay, independent state/device comparison,
override/original-call coverage, invalidation controls, product link/selector proof, and released-
host qualification remain required. The deleted static machinery must not return.
