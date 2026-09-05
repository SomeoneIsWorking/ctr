---
id: 24
title: CTR product needs Lightrec and explicit executor exits
status: open
symptom: Static execution and FrameCompleted unwinding are removed, but the replacement Lightrec path is not yet verified on the preserved frontier
state_items: S009,S010
tags: ctr,dynarec,lightrec,executor,frame-loop,migration
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

CTR was composed around a static dispatcher and local `FrameCompleted` exception unwinding. The
break-first migration removed both, while the replacement Lightrec implementation, overlay image
activation, and nested-exit proof remain incomplete.

## Required resolution

Consume psxport's per-`Core` Lightrec executor and map authenticated resident/overlay images directly.
Register native overrides by image generation plus guest address; original calls bypass only the
active override and execute through Lightrec; overlay replacement invalidates affected blocks.

Each former `FrameCompleted` site records its exact title continuation/phase and requests a typed
executor exit. Lightrec must return normally at a safe dispatcher boundary with synchronized
state. The frame driver validates the result, lets scoped driver/override ownership unwind normally,
finishes exactly one field/presentation fence, and increments its counter. Unknown exits,
unexpected normal return, and stale continuation state remain fatal.

The first discriminator reaches issue 0023's current boundary with the existing native owners and
nonzero Lightrec execution. Representative interactive gameplay, independent state/device
comparison, override/original-call coverage, invalidation controls, product link/selector proof, and
released-host qualification remain required. The deleted static machinery must not return.
