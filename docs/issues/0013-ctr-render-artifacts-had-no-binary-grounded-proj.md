---
id: 13
title: CTR render artifacts had no binary-grounded projection or primitive producer boundary
status: investigating
symptom: The port had GTE/OT output terminology but no verified function that produced projection state or graphics packets, so native renderer, widescreen, and interpolation work had no honest source boundary.
state_items: S004, S005, S006, S007
tags: ctr05,projection,native-renderer,widescreen,interpolation
created: 2026-08-26
updated: 2026-09-04
---

## Root cause

Output-side GTE registers and ordering-table packets erase producer ownership. The existing frontier correctly refused to treat those artifacts as native input, but no identity-gated static census or decompiled producer boundary existed.

## Current finding

`tools/measure_render_frontier.py` now identifies exact libgte leaves `0x8007781C`/`0x8007782C`, projection producer `0x80042910`, and the address-registered `lensflare` primitive producer `[0x80024C4C,0x80025138)`. The primitive producer runs MVMVA/three RTPT commands, stores SXY into four tagged packets, and inserts them into the ordering table. CTR binds only the generic projection leaves through the framework-owned typed HLE seam.

## Remaining resolution boundary

Static evidence cannot establish execution order or frame ownership. First reproduce issue 0023's
current frontier through the native/Lightrec product; then a serialized dynamic trace must prove
which registrar/indirect callback and projection producer execute in representative gameplay. Native
render ownership begins from that observed producer and compares its override against a Lightrec
original call. Widescreen changes native camera/projection inputs, and interpolation retains
previous/current native transforms rather than quantized GTE output. Do not extend or rerun the
static route.
