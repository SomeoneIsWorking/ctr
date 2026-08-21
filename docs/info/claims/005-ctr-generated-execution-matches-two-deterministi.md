---
id: C005
kind: claim
status: holds
created: 2026-08-21
tags:
depends: game/core/crt0_port_trace.cpp#modelInitHeapReturn, tools/compare_crt0_trace.py#main, CMakeLists.txt
reconfirmed: 2026-08-21
verified_at: 2026-08-21 11:21:06
---

## Claim

CTR generated execution matches two deterministic true-oracle runs through the A(39h) InitHeap return and the first subsequent call at 0x8003C58C.

## Evidence

On the identity-bound USA SCUS_944.26, compare_crt0_trace.py --post-init-heap ran the independent Mednafen CPU twice and required identical boundary state and step counts. It verified A(39h), applied only explicit v0=0 return semantics, and compared initial call, modeled return, and subsequent call against shipping generated execution: 108/108 fields agreed. The both-answer run forced post.gp=0 and reported exactly one named disagreement, 107/108.

## What would falsify it

A change to game/core/crt0_port_trace.cpp, tools/compare_crt0_trace.py, the framework oracle resume/trace implementation, generated substrate, target executable identity, or A(39h) semantics; falsify if repeat oracles differ or any of 108 fields diverges.

## Re-confirmed 2026-08-21

Re-verified after psxport 9f1bb927 landed: a fresh Clang 22.1.8 build against the recorded pin produced two identical true-oracle runs, 108/108 generated agreement at post-InitHeap call 0x8003C58C, and the forced post.gp=0 control produced exactly one named disagreement (107/108). The preserved pre-BIOS control remained 34/34 with a forced 33/34 opposite.

## Re-confirmed 2026-08-21

Post-landing two oracle runs remained identical and generated execution agreed 108/108 after InitHeap; forced post.gp=0 produced 107/108.
