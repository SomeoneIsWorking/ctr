---
id: C017
kind: claim
status: holds
created: 2026-08-26
tags: ctr04,generated,boot-frontier
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, game/core/crt0_port_trace.cpp#main, game/core/bootstrap_frontier.cpp#runBootstrapToSupportedFrontier
reconfirmed: 2026-08-26
verified_at: 2026-08-26 23:48:29
---

## Claim

CTR's freshly emitted generated startup reaches the reproducible indirect-dispatch boundary at 0x800772E0 with oracle-identical CPU state on clean framework 51a60926

## Evidence

On identity-verified SCUS_944.26, psxport recompiler 2026-08-26.14 emitted 1,448 functions in eight shards. The exact 51a609267a95ad2f08426df6fe83b57f3bff3c14 Clang build ran the complete chained oracle/generated gate through ctr04_startup_init_dispatch_next_call_check: repeat oracle captures were deterministic, all 34 boundary fields agreed at pre-instruction PC 0x800772E0, and forced resident.gp=0 produced the sole named 33/34 mismatch. The no-launch prepare-only path built ctr_port with its supported stop restored to 0x800772E0.

## What would falsify it

The executable identity, checked replay inputs, generated router, 0x800772E0 stop boundary, or any compared field changes; repeated oracle captures diverge; or the forced control is no longer isolated

## Re-confirmed 2026-08-26

Reverified 2026-08-26 on identity-checked SCUS_944.26 with a fresh Clang build against exact psxport 99a42aa396eb810b7872c17bcc4610d21252a61c: recompiler 2026-08-26.14 emitted 1,448 functions in eight shards, the complete chained oracle/generated gate reached pre-instruction 0x800772E0 with deterministic 34/34 agreement, the forced resident.gp opposite produced the sole 33/34 mismatch, and the no-launch prepare-only path built ctr_port at that supported frontier.
