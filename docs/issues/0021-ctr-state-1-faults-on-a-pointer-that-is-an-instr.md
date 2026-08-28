---
id: 21
title: CTR state-1 faults on a pointer that is an instruction word
status: open
symptom: UNMAPPED RAM read32 @ 0x3C02A001 inside gen_func_8006D79C, called from state-1 0x8003B934
state_items: S003,S005
tags: frontier,gte,scratchpad,state-1
created: 2026-08-28
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

## What is not yet known

Whether the null arguments are the fault (a caller reading an object that a later BIGFILE module was
supposed to populate) or a symptom, and whether any of the four module loads after entry 233 —
sectors 56636 to `0x8010F104` and two loads to `0x801217D8` — is a code overlay this port has not
declared. Nothing here claims a cause.
