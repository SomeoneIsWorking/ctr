---
id: 24
title: CTR product needs Lightrec and explicit executor exits
status: open
symptom: CTR still dispatches generated guest functions and uses C++ FrameCompleted unwinding where the product requires a JIT-safe bounded executor exit
state_items: S009,S010
tags: ctr,dynarec,lightrec,executor,frame-loop,migration
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

CTR was composed around psxport's retired static dispatcher. Ordinary retail calls and native-owner
original calls resolve to generated host functions, and `CtrFrameDriver` escapes waits, service
points, and field completion by throwing local `FrameCompleted` through those host frames. A Lightrec
product cannot rely on C++ exception or `longjmp` unwinding through JIT code.

## Required resolution

Consume psxport's per-`Core` Lightrec executor and map authenticated resident/overlay images directly.
Register native overrides by image generation plus guest address; original calls bypass only the
active override and execute through Lightrec; overlay replacement invalidates affected blocks.

Each current `FrameCompleted` site must instead record its exact title continuation/phase and request
a typed executor exit. Lightrec returns normally at a safe dispatcher boundary with synchronized
state. The frame driver validates the result, restores the captured diagnostic-depth invariant,
finishes exactly one field/presentation fence, and increments its counter. Unknown exits,
unexpected normal return, and stale continuation state remain fatal.

The first discriminator reaches issue 0023's current boundary with the existing native owners and
nonzero Lightrec execution. It does not authorize deletion. Representative interactive gameplay,
independent state/device comparison, override/original-call coverage, invalidation controls, product
link/selector proof, and released-host qualification must pass before the static path is removed.
