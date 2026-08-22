---
id: 7
title: CTR replay stops before startup state-zero service thunk
status: resolved
symptom: The verified CTR replay reaches 0x8001D06C but does not prove its idle return, state-zero dispatch, or the first initialization service call.
tags: ctr,oracle,replay,boot,bios
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

The previous proof stopped before `0x8001D06C`, whose branch depends on BSS pending-work word
`0x8008D6B8`. Its return reaches a jump-table dispatch driven by mode word `0x8008D0F4`; neither the
executed islands, table entry, nor state pointer had been established as replay inputs.

## What was tried / dead ends

The external vector at `0xA0` was not treated as executable game code. Ghidra and exact MIPS words
show `0x800718BC` is only an A(2Bh) thunk, so replay cannot continue through it with a register-only
return: BIOS `memset` mutates `0x2584` bytes of RAM.

## Resolution

### Resolution (2026-08-22)
Ghidra and exact executable words proved the BSS-zero return, mode-zero jump-table selection, and callsite. The bounded ordinal-4 gate checks every code/data input and agrees 34/34 at executable A(2Bh) thunk 0x800718BC; forced gp gives 33/34. The next honest gap is the external memset RAM mutation.
