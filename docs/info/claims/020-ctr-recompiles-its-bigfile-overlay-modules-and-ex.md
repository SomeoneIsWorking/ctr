---
id: C020
kind: claim
status: holds
created: 2026-08-28
tags: ctr04,overlays,bigfile,emitter,frontier
depends: tools/extract_overlays.py, tools/emit_substrate.py, game/recomp_seeds.json#overlay_bases
---

## Claim

CTR reproducibly extracts its BIGFILE.BIG code modules, recompiles them as overlay modules, and executes them, advancing retail main from the screen loader into state 1

## Evidence

`BIGFILE.BIG` verified at 270,360,576 bytes and SHA-256
`98779a7990f395fe3620481a1b5478149c88ff17fffa64905369f3e46368fce7`. Its index parses as 608
monotonic, non-overlapping (sector offset, byte size) entries from word 2. `PSXPORT_DEBUG=cd` over a
real boot named entries 225, 226 and 233 loading to 0x8009F6FC, 0x800A0CB8 and 0x800AB9F0, and the
missed call 0x800B0B38 lies inside entry 233. The emitter recompiled those three images as 1, 270 and
152 functions, and the next serialized real-disc run resolved 0x800B0B38, ran the loader's five
pointer-only completion callbacks, performed four further module loads, and reached state-1
`0x8003B934`. Extraction is not optional: `emit_substrate.py` refuses when a declared overlay
produced no image. `ctr_overlay_extract_selftest` passes 9/9 covering index parsing, a short index,
an empty index, overlapping entries, a bad stem shape, and both classifier verdicts; `verify` and
CTest 9/9 pass on Clang.

## What would falsify it

The archive identity, its index layout, the three measured load bases, or the entry ids change; a run
shows an overlay function executing at a different base than declared; or the emitter's nested-range
report fires for an image sliced at its index byte size
