---
id: 20
title: CTR calls overlay code the emitter never discovered
status: resolved
symptom: recomp-MISS at 0x800B0B38 from caller ra=0x800368BC once the load pipeline reaches stage 5
state_items: S003,S005
tags: overlays,recompiler,emitter,frontier
created: 2026-08-28
updated: 2026-08-28
---

## Symptom

With the libcd read-completion callback delivered (issue 0019), the state-3 screen loader advances to
stage 5 and the guest calls 0x800B0B38. `tools/emit_substrate.py` reports `[overlays] 0 overlay
module(s)`, and the executable's text extent ends at 0x8008D800, so this is code loaded from the disc
at run time with no recompiled body. The product fails fast, by design, with a recomp-MISS.

`FUN_80033610` case 4 and case 8 also call 0x800B4364, 0x800B446C, 0x800B4430, 0x800B43F4,
0x800B8558, 0x800B44A8 and 0x800B4470 — the same region, reached through several distinct paths.

## What is not yet known

Which disc file backs that load, its load address and extent, whether one module or several overlap
that region, and whether the emitter can discover their entries statically or needs the measured load
map. None of that is established yet; nothing here claims a cause beyond "no body was emitted".

## Root cause

CTR keeps its overlays inside `BIGFILE.BIG` rather than as separate disc files, so the shipping
emitter's `--overlays DIR` input did not exist and nothing was ever emitted for that address space.
The archive index is a table of (sector offset, byte size) pairs starting at word 2 of the file: 608
entries, monotonic and non-overlapping. `PSXPORT_DEBUG=cd` over a real boot named the three loads the
screen loader performs — entries 225, 226 and 233 to 0x8009F6FC, 0x800A0CB8 and 0x800AB9F0 — and
0x800B0B38 lies inside entry 233's image.

## Fix

`tools/extract_overlays.py` verifies the archive by size and SHA-256, parses and validates the index,
and slices the entries the seed file names into `scratch/raw/ctr/overlays/BF<id>.BIN` at their exact
byte size. A stem names an ARCHIVE ENTRY, not a run. `tools/emit_substrate.py` runs it and refuses if
a declared overlay produced no image, so a missing archive can never degrade silently into a product
that fails much later with an unexplained recomp-MISS. `game/recomp_seeds.json` records the three
measured bases with their rationale.

The emitter then recompiled 1, 270 and 152 functions for the three modules. Sizing matters: reading
the sector-padded image instead of the exact size made entry 226 overlap entry 233's base and the
emitter correctly reported the nested range, so the extractor uses the index's byte size.

## Evidence

The next real-disc run resolved 0x800B0B38, advanced past the loader's completion callbacks (five
pointer-only entries seeded after the miss moved to 0x80031B00), performed four further module loads,
and reached main state 1. `ctr_overlay_extract_selftest` proves index parsing, four refusals, and BOTH
classifier verdicts; `verify` and CTest 9/9 pass.

## Exact remaining frontier

Execution now faults inside `0x8006D79C` (a scratchpad register-spill helper) called from state-1
`0x8003B934`, reading through a pointer that is a MIPS instruction word rather than data. Issue 0021
owns it.
