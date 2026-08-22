---
id: C010
kind: claim
status: holds
created: 2026-08-22
tags:
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, CMakeLists.txt
reconfirmed: 2026-08-22
verified_at: 2026-08-22 18:35:49
---

## Claim

CTR generated execution matches deterministic oracle replay through the state-zero initialization dispatch at executable A(2Bh) thunk 0x800718BC on 34/34 boundary fields.

## Evidence

Ghidra and exact executable words established the BSS-zero 0x8001D06C return, mode-zero jump-table entry 0x8003C614, destination pointer 0x80096B20, and complete callsite. Two original captures and two call-ordinal-4 replays were deterministic; generated execution agreed 34/34 at 0x800718BC and forced resident.gp=0 produced 33/34.

## What would falsify it

The claim is falsified if executable identity, any checked path byte/data input, repeated oracle boundary, target 0x800718BC, arguments, or any generated boundary field changes.

## Re-confirmed 2026-08-22

Fresh Clang rebuild against recorded psxport ad5cf802; normal verify passed, embedded CTest passed 87/87, comparator selftest passed 18/18, exact executable repeated oracle/replay captures were deterministic, generated execution agreed 34/34 at 0x800718BC, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Post-commit Clang verification passed 87/87 CTest with exact ad5cf802 pin; real SCUS_944.26 oracle/generated windows remained deterministic, comparator controls passed 18/18, the new 0x800718BC boundary agreed 34/34, and forced resident.gp produced the named 33/34 difference.
