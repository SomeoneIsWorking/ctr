---
id: 5
title: CTR bounded replay stopped at runtime initializer entry
status: resolved
symptom: The verified CTR continuation captures 0x800779E4 but does not execute it or reach the next resident call.
tags: ctr,oracle,replay,boot
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

The replay proof covered only the store-only caller prefix ending at the call to `0x800779E4`.
Nothing established the initializer's complete instruction/data footprint or the caller path after it,
so continuing execution would have depended on unchecked executable state.

## What was tried / dead ends

No output shortcut was attempted. Static decompilation first established the initializer's branch,
constructor-count, initialized-data, and return behavior; that evidence made a bounded continuation
possible without synthesizing game output or assuming unverified memory.

## Resolution

### Resolution (2026-08-22)
Ghidra proved 0x800779E4 is a one-time runtime initializer whose constructor count is zero; its two initialized-data inputs remain unchanged before crt0's BSS range. The replay now byte-checks the initializer and caller continuation, checks both data words, excludes both stack-write spans, and captures call ordinal 2 at 0x80032DC0. Two oracle runs agreed with generated execution 34/34; forced gp produced 33/34.
