---
id: C005
kind: claim
status: holds
created: 2026-08-21
tags:
depends: game/core/crt0_port_trace.cpp#modelInitHeapReturn, tools/compare_crt0_trace.py#main, CMakeLists.txt
reconfirmed: 2026-08-22
verified_at: 2026-08-22 14:18:17
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

## Re-confirmed 2026-08-21

Re-verified after framework deterministic CD pacing landed at exact ce2c83ad: two true-oracle post-InitHeap runs remained deterministic, generated execution agreed 108/108 at 0x8003C58C, and forced post.gp=0 produced exactly one named disagreement at 107/108.

## Re-confirmed 2026-08-21

Post-landing ce2c83ad repeated true-oracle and generated comparison retained 108/108 at the post-InitHeap boundary with the named forced mismatch.

## Re-confirmed 2026-08-21

Re-verified against exact psxport 3418a79b624765614f3f198dc1e89632e1e650f0: two true-oracle post-InitHeap runs produced identical three-boundary evidence, generated execution agreed 108/108 at 0x8003C58C, and forced post.gp=0 produced exactly one named disagreement at 107/108.

## Re-confirmed 2026-08-22

Fresh real-disc post-InitHeap chain through CtrRuntime produced two deterministic oracle captures, 108/108 generated agreement, and forced post.gp=0 produced 107/108.
