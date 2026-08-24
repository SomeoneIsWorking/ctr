---
id: C011
kind: claim
status: holds
created: 2026-08-24
tags:
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, game/core/crt0_port_trace.cpp#modelMemsetReturn, CMakeLists.txt
reconfirmed: 2026-08-24
verified_at: 2026-08-24 20:05:45
---

## Claim

CTR generated execution matches deterministic oracle replay through the explicitly modeled external A(2Bh) memset leaf to the entry of the following executable swap, agreeing 34/34 at `0x80077CD8` (the state-zero case's next call after the memset).

## Evidence

The bounded replay poisons the complete destination `[0x80096B20,+0x2584)` with 0xA5 before entry and byte-checks thunk `0x800718BC` before redirecting it to a register-preserving byte loop, so a missing or wrong modeled write cannot pass; the generated tracer independently requires the poison before applying the same leaf contract (`modelMemsetReturn`). Two original captures and two call-ordinal-5 replays were deterministic; generated execution agreed 34/34 at `0x80077CD8` and forced `resident.gp=0` produced 33/34. Verified 2026-08-24 by the full asset-gated chain from real-disc provisioning (SHA-256 `7b4aac0b…` reproduced) through `ctr04_startup_post_memset_next_call_check`.

## What would falsify it

The claim is falsified if executable identity, any checked path byte/data input, the memset thunk bytes, the poison or fill values, repeated oracle boundary determinism, target `0x80077CD8`, or any generated boundary field changes.

## Re-confirmed 2026-08-24

Corrected no-stack-residue A(2Bh) model re-ran through the real SCUS_944.26 asset gate on recorded psxport d2266f4b: SHA-256 7b4aac0b… reproduced, two original and two call-ordinal-5 oracle captures were deterministic, generated state agreed 34/34 at 0x80077CD8, and forced resident.gp produced 33/34.

## Re-confirmed 2026-08-24

Isolated serialized real SCUS_944.26 gate process 468226 on psxport bc8c8897 reproduced SHA-256 7b4aac0b..., deterministic oracle/replay, 34/34 at 0x80077CD8 after corrected no-stack-residue memset, and named 33/34 forced-gp disagreement. An earlier overlapped run is excluded from acceptance evidence.
