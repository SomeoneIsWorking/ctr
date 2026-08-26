---
id: C007
kind: claim
status: holds
created: 2026-08-22
tags:
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, game/core/crt0_port_trace.cpp#restoreResidentState, CMakeLists.txt
reconfirmed: 2026-08-26
verified_at: 2026-08-26 23:28:39
---

## Claim

CTR generated execution matches deterministic oracle replay through the runtime initializer at call 0x80032DC0 on 34/34 boundary fields.

## Evidence

Ghidra established the complete traversed code/data footprint. The comparator verified the exact 7b4aac0b... executable, two original post-InitHeap captures, two call-ordinal-2 replay captures, all code ranges, both initialized-data words, and both stack-write spans; generated execution agreed 34/34 and forced resident.gp=0 produced 33/34.

## What would falsify it

The claim is falsified if the executable identity, any checked code/data input, either repeat oracle boundary, target 0x80032DC0, or any generated boundary field changes.

## Re-confirmed 2026-08-22

Reconfirmed with Clang 22.1.8 against CTR's exact recorded psxport 3418a79b: normal verify passed, two original oracle captures and two call-ordinal-2 bounded replays were deterministic, generated execution agreed 34/34 at 0x80032DC0, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Fresh real-disc runtime-initializer replay through CtrRuntime produced deterministic oracle captures and 34/34 generated agreement at 0x80032DC0; forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Against exact landed and recorded psxport ad5cf802, repeated original and call-ordinal-2 replay captures were deterministic, generated execution agreed 34/34 at 0x80032DC0, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Current repeated original and call-ordinal-2 replay captures were deterministic, generated execution agreed 34/34 at 0x80032DC0, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Post-commit Clang verification passed 87/87 CTest with exact ad5cf802 pin; real SCUS_944.26 oracle/generated windows remained deterministic, comparator controls passed 18/18, the new 0x800718BC boundary agreed 34/34, and forced resident.gp produced the named 33/34 difference.

## Re-confirmed 2026-08-24

Post-landing isolated real-data chain reconfirmed the runtime-initializer continuation at 34/34 and forced gp at 33/34

## Re-confirmed 2026-08-26

Reverified 2026-08-26 on identity-checked SCUS_944.26 with a clean Clang build against exact psxport 51a609267a95ad2f08426df6fe83b57f3bff3c14: the complete chained oracle/generated target through 0x800772E0 passed every named deterministic boundary and its forced-opposite control using freshly emitted recompiler 2026-08-26.14 output.
