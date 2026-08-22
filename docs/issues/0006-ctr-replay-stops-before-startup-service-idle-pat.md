---
id: 6
title: CTR replay stops before startup service idle path
status: resolved
symptom: The verified CTR replay reaches 0x80032DC0 but does not prove that its memory-reading startup path returns to caller and reaches the next call.
tags: ctr,oracle,replay,boot
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

The prior boundary stopped before a startup service whose idle path reads both executable-backed
initialized data and crt0-zeroed BSS. Nothing had established the exact path or its complete input
footprint. The first extended replay then placed its own trampoline across checked BSS input
`0x8008D708`, so the instrument's `lui` instruction manufactured the apparent `v0` divergence.

## What was tried / dead ends

No renderer or later-output bypass was attempted. Ghidra first established the service's startup
path. The initial 33/34 result was rejected because the unexpected value `0x3C048009` was an
instruction word rather than plausible game state; correlating that word with the replay image
localized the overlap in the trampoline planner.

## Resolution

### Resolution (2026-08-22)
Ghidra proved 0x80032DC0 startup path reads request word 1, BSS loading flag 0, request count 0, and timestamp 0, then returns to call 0x8001D06C. The first exact replay falsely reported v0 divergence because its trampoline occupied 0x8008D6F0..0x8008D7FF and overwrote checked input 0x8008D708. resident_replay now reserves every expected code/data range when placing the trampoline; the real gate moved it to 0x8008CE5C, produced deterministic 34/34 agreement, and the forced gp control produced 33/34.
