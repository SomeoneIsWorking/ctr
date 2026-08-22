---
id: C006
kind: claim
status: holds
created: 2026-08-21
tags:
depends: tools/resident_replay.py#build_replay, tools/compare_crt0_trace.py#main, game/core/crt0_port_trace.cpp#restoreResidentState, CMakeLists.txt
reconfirmed: 2026-08-22
verified_at: 2026-08-22 18:35:49
---

## Claim

CTR generated execution matches the deterministic true oracle at the first resident call inside 0x8003C58C, reaching 0x800779E4 with 34/34 boundary fields.

## Evidence

On the identity-bound USA executable, the comparator reproduced post-InitHeap state twice, built an exact-register replay only after validating the Ghidra-proven store-only original prefix, and used canonical oracle_trace --capture-call 1 twice to discover 0x800779E4. Generated execution agreed 34/34; forced resident.gp=0 produced exactly one named disagreement, 33/34.

## What would falsify it

Falsify if the executable identity or exact 0x8003C58C..0x8003C5AC prefix changes, either repeat oracle differs, canonical capture reaches a different boundary, or any of 34 generated boundary fields diverges.

## Re-confirmed 2026-08-21

Re-verified in a clean Clang 22.1.8 configure against recorded psxport pin 9f1bb927: two original
oracle runs and two canonical `--capture-call 1` replay runs were deterministic, generated execution
agreed 34/34 at `0x800779E4` with `ra=0x8003C5B0`, forced `resident.gp=0` produced 33/34, the
production replay selftest passed 11/11 including main-RAM alias exclusion and missing-zero-run
refusal, `cpp_policy` passed format/size/clang-tidy 1/1, and normal `verify` passed.

## Re-confirmed 2026-08-21

Re-verified after framework deterministic CD pacing landed at exact ce2c83ad: two original oracle runs and two canonical --capture-call 1 replay runs remained deterministic; generated execution agreed 34/34 at 0x800779E4 with ra=0x8003C5B0; forced resident.gp=0 produced 33/34; replay selftest passed 11/11; cpp_policy passed format/size/clang-tidy 1/1; normal verify confirmed the build and recorded pin are ce2c83ad.

## Re-confirmed 2026-08-21

Post-landing bounded resident replay selftest passed 11/11 and real generated/oracle execution agreed 34/34 at 0x800779E4; forced resident gp reported 33/34.

## Re-confirmed 2026-08-21

Re-verified against exact psxport 3418a79b624765614f3f198dc1e89632e1e650f0: two original oracle runs and two canonical --capture-call 1 bounded-replay runs were deterministic; generated execution agreed 34/34 at 0x800779E4 with ra=0x8003C5B0; forced resident.gp=0 produced 33/34; replay selftest passed 11/11; cpp_policy passed format/size/clang-tidy 1/1; normal verify passed 8/8 and confirmed the exact recorded pin.

## Re-confirmed 2026-08-22

Fresh real-disc bounded replay through CtrRuntime produced deterministic oracle captures and 34/34 generated agreement at 0x800779E4; forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Against exact landed and recorded psxport ad5cf802, repeated original and call-ordinal-1 replay captures were deterministic, generated execution agreed 34/34 at 0x800779E4, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Current repeated original and call-ordinal-1 replay captures were deterministic, generated execution agreed 34/34 at 0x800779E4, and forced resident.gp=0 produced 33/34.

## Re-confirmed 2026-08-22

Post-commit Clang verification passed 87/87 CTest with exact ad5cf802 pin; real SCUS_944.26 oracle/generated windows remained deterministic, comparator controls passed 18/18, the new 0x800718BC boundary agreed 34/34, and forced resident.gp produced the named 33/34 difference.
