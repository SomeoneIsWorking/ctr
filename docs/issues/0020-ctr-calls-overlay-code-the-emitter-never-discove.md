---
id: 20
title: CTR calls overlay code the emitter never discovered
status: open
symptom: recomp-MISS at 0x800B0B38 from caller ra=0x800368BC once the load pipeline reaches stage 5
state_items: S003,S005
tags: overlays,recompiler,emitter,frontier
created: 2026-08-28
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
