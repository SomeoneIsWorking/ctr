---
id: 3
title: CTR true oracle stopped before InitHeap return
status: resolved
symptom: CTR differential ended at the first call and could not compare the next game call after A(39h)
tags: oracle,bios,ctr-04,determinism
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

The independent CPU intentionally maps no BIOS, and oracle_trace had no checked continuation
surface. Running past target `0xA0` therefore executed zeroed vector memory rather than Sony A(39h).

## What was tried / dead ends

Treating the zeroed vector as executable reference behavior cannot establish a BIOS return contract;
the trace correctly stopped before that path could be used as evidence.

## Resolution

### Resolution (2026-08-21)
The framework now validates an explicit target/RA return model, preserves CPU time/state, and
captures the next boundary. CTR owns the A(39h) `v0=0` policy, repeats the oracle for determinism,
and matches generated execution 108/108 at `0x8003C58C`; forced `post.gp=0` reports 107/108.
