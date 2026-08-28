---
id: 21
title: CTR state-1 faults on a pointer that is an instruction word
status: resolved
symptom: UNMAPPED RAM read32 @ 0x3C02A001 inside gen_func_8006D79C, called from state-1 0x8003B934
state_items: S003,S005
tags: frontier,gte,scratchpad,state-1
created: 2026-08-28
updated: 2026-08-28
---

## Symptom

With BIGFILE overlays recompiled (issue 0020), the product advances through the screen loader into
main state 1 and then fails fast: `FATAL: UNMAPPED RAM read32 @ 0x3C02A001 (phys 0x1C02A001)`. The
host backtrace is `func_8003CB0C -> func_8003CC98 -> gen_func_8003C9EC -> gen_func_8003B934 ->
gen_func_8006D79C`, with `ra=0x8003C07C`, `a0=0`, `a1=0`, `a2=0xFFFFFFF8`, `s0=0x80100548`,
`s1=0x80096B20`.

`0x3C02A001` is itself a MIPS instruction word (`lui v0, 0xA001`), and the register dump shows it
sitting at guest RAM `0x28` — so a pointer was read out of low RAM, or an address lost its
`0x1F800000` scratchpad base, and was then dereferenced.

## What is known

Ghidra decompiles `0x8006D79C` as a scratchpad register-spill helper: it writes `s0`-`s7` and the
return address to `0x1F800000`-`0x1F800028`, sets GTE control register `0x4000` from
`(a0 & 7) << 9`, and then calls `0x8006DB7C` `a1` times with a rotating counter. It was entered with
`a0 = a1 = 0`, so the caller supplied a null count and mode.

## Root cause

Not a missing overlay: the three later module loads classify as data, with zero `jr ra` words. The
defect was the DELIVERY TIME of the libcd read-completion callback added in issue 0019.

Retail cannot deliver that callback inside CdRead, and the loader depends on it. `FUN_80031E00`
calls `FUN_800321B4` with `dest = 0` (allocate) and only then stores the returned buffer into the
queue entry (`DAT_80083A48`, entry+0xC). The completion chain `FUN_80031D30` reads that same field to
run the module's relocation pass and then hands it to the user callback, and `FUN_80031A78` stores it
into `gp+0x120`. Delivering inline meant the whole chain ran while entry+0xC was still 0: the
relocation pass was skipped, `gp+0x120` became 0, `FUN_80033610` case 7 set `game+0x160 = 0`, and
state-1 `FUN_8003B934` then read `*(0 + 40)` — guest RAM `0x28`, which holds `0x3C02A001` — and passed
it to `0x8006D79C` as the array pointer it walks.

Watchpoints on `gp+0x11C..gp+0x18C` proved the sequence: the loader ran its full 0-9 stage chain,
`0x80031A78` stored 0 into `gp+0x120`, and main advanced to state 1 before faulting.

## Fix

`DiscReadOwner` now records that a completion is OWED and delivers it at a seam where the issuing
call has unwound: the title frame driver's per-field service point, and before any subsequent read,
because one guest drive cannot have two transfers in flight. A second completion arriving with one
still owed aborts rather than losing it. The runtime test requires both halves — a registered
callback must NOT run inside the read, and a delivered one must run exactly once with `CdlComplete`
and leave v0/ra untouched.

## Evidence

The next real-disc run passed the null dereference, performed the later module loads, presented at
least 16 fields, and reached the hand-written GTE library. It now fails fast on a computed jump the
recompiler did not resolve — `0x8006ACE0` from `0x8006C948`, which is `jr t2` to a caller-supplied
continuation. Issue 0022 owns that. `verify` and CTest 9/9 pass.
