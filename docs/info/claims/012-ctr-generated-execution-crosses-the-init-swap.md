---
id: C012
kind: claim
status: holds
created: 2026-08-24
tags:
depends: tools/compare_crt0_trace.py#main, tools/resident_replay.py#build_replay, docs/re-frontier.md#ctr-04
reconfirmed: 2026-08-24
verified_at: 2026-08-24 20:09:44
---

## Claim

CTR generated execution matches deterministic oracle replay across the modeled memset, the caller jal island `0x8003C62C..0x8003C630`, and the whole executable swap function `0x80077CD8`, agreeing 34/34 at the entry of dispatch thunk `0x800771C4` (call ordinal 6).

## Evidence

Exact executable words prove the swap is a five-instruction leaf whose delay-slot store writes a0 into initialized word `0x8008C0B4` (file value 0) while v0 returns the previous word; Ghidra's `FUN_80077cd8` matches. The window byte-checks the post-memset jal island and the whole swap function, checks data word `0x8008C0B4 == 0`, and excludes all stack-write spans. Two original captures and two call-ordinal-6 replay oracle runs were deterministic (boundary step 48203); generated execution agreed 34/34 at `0x800771C4` — including `v0=0`, the swapped-out word — and forced `resident.gp=0` produced 33/34. Verified 2026-08-24 via `ctr04_startup_init_swap_next_call_check` chained behind real-disc provisioning.

## What would falsify it

The claim is falsified if executable identity, any checked island or data input changes, the swap function bytes change, repeated oracle boundaries stop being deterministic, the ordinal-6 target stops being `0x800771C4`, or any generated boundary field diverges.

## Re-confirmed 2026-08-24

Full corrected-model chained gate on real SCUS_944.26 and recorded psxport d2266f4b: two call-ordinal-6 oracle captures were deterministic at step 48203, generated state agreed 34/34 at 0x800771C4 after executing 0x80077CD8, and forced resident.gp produced 33/34.

## Re-confirmed 2026-08-24

Isolated serialized real SCUS_944.26 gate process 468226 on psxport bc8c8897 reproduced SHA-256 7b4aac0b..., deterministic ordinal-6 oracle/replay at 0x800771C4, 34/34 generated agreement, and named 33/34 forced-gp disagreement. An earlier overlapped run is excluded from acceptance evidence.

## Re-confirmed 2026-08-24

Post-landing isolated PID 468226 reconfirmed init-swap dispatcher 0x800771C4 at 34/34 and forced gp 33/34; overlapped prior run was discarded
