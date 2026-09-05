---
id: 21
title: CTR state-1 fault was caused by early libcd callback delivery
status: resolved
symptom: State 1 dereferenced instruction word 0x3C02A001 as a pointer after the screen loader completed
state_items: S003,S005
tags: frontier,gte,scratchpad,state-1,libcd
created: 2026-08-28
resolved: 2026-08-28
---

## Root cause

This was not a missing runtime module. `FUN_80031E00` calls `FUN_800321B4` with `dest=0`, then stores
the returned allocation in queue entry `DAT_80083A48+0xC`. The completion chain `FUN_80031D30`
reads that field to run relocation and later publishes it through `gp+0x120`. Delivering the libcd
completion inside CdRead ran the chain before the caller stored the allocation: relocation was
skipped, `gp+0x120` became zero, and state 1 read guest RAM `0x28` as a pointer. That word is the
instruction `0x3C02A001`.

## Resolution and evidence

`DiscReadOwner` records an owed completion and delivers it only after the issuing call has unwound,
at CTR's per-field service seam and before a subsequent read. A second completion while one is owed
is fatal. The production-seam test requires that a registered callback does not run inline, then runs
exactly once with `CdlComplete` while preserving the interrupted return state.

The next observed run crossed this fault, performed later module loads, presented at least 16 fields,
and entered CTR's hand-written GTE library. Issue 0022 records the alternate-link convention found at
that later boundary.
