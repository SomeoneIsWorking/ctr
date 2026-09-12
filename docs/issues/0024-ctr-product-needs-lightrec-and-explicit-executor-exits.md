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
next discriminator must explain that budget use before reaching issue 0023's current boundary with
the existing native owners and nonzero Lightrec execution. Representative interactive gameplay, independent state/device
comparison, override/original-call coverage, invalidation controls, product link/selector proof, and
released-host qualification remain required. The deleted static machinery must not return.
