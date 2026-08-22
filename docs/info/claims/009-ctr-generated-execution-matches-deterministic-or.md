---
id: C009
kind: claim
status: holds
created: 2026-08-22
tags:
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, CMakeLists.txt
reconfirmed: 2026-08-22
verified_at: 2026-08-22 18:10:48
---

## Claim

CTR generated execution matches deterministic oracle replay through the startup service idle path at call 0x8001D06C on 34/34 boundary fields.

## Evidence

Ghidra decompiled 0x80032DC0 and established its startup idle reads and exact control-flow islands. The identity-gated comparator checked the exact 7b4aac0b... executable, all code/data inputs, two original oracle captures, two call-ordinal-3 replay captures, and generated execution: 34/34 agreed at 0x8001D06C; forced resident.gp=0 produced 33/34. The trampoline planner selftest proves checked data cannot be occupied by replay code.

## What would falsify it

The claim is falsified if the executable identity, any checked path byte or data input, either repeat oracle boundary, target 0x8001D06C, trampoline evidence exclusion, or any generated boundary field changes.

## Re-confirmed 2026-08-22

Fresh Clang consumer rebuild against the quiescent shared framework; exact 7b4aac0b... executable produced deterministic repeated oracle/replay captures, 34/34 generated agreement at 0x8001D06C, forced resident.gp=0 produced 33/34, comparator selftest 17/17, and embedded CTest 87/87.

## Re-confirmed 2026-08-22

Verified against exact landed and recorded psxport ad5cf802: normal verify passed its provenance guard and all asset-free contracts, Clang policy passed, embedded CTest passed 87/87, exact 7b4aac0b... executable produced deterministic 34/34 agreement at 0x8001D06C, and forced resident.gp=0 produced 33/34.
