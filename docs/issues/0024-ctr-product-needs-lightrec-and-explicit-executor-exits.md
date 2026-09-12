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
per dispatch and the CTR frame driver treated any budget exit as fatal. The smallest next runtime
discriminator is the typed exit detail/cycle count plus `t3`, `t9`, `s6`, stack pointer, and return
address at the stop, with bounded counts at `0x8006A610`, `0x8006A57C`, and `0x8006A6B0` across
one continued budget slice. Advancing `t9` and a later frame exit would identify a finite quantum;
a repeated descriptor/pointer without expected progress would redirect investigation to the render
list or guest control flow. Neither explanation is established by the saved trace. Resolve this
before reaching issue 0023's current boundary with the existing native owners and nonzero Lightrec
execution. Representative interactive gameplay, independent state/device comparison,
override/original-call coverage, invalidation controls, product link/selector proof, and released-
host qualification remain required. The deleted static machinery must not return.
