---
id: 9
title: Modeled memset leaves guest-stack residue
status: resolved
symptom: The oracle-side A(2Bh) memset replay model writes temporary register saves below guest sp, but the generated-side model mutates only the requested destination.
tags: ctr,oracle,replay,bios,memset
created: 2026-08-24
updated: 2026-08-24
---

## Root cause

The injected MIPS leaf preserved `v1`, `t0`, and `t1` in a temporary frame below guest `sp`, so its
register result matched while RAM retained three saves that the C++ model never wrote. The boundary
comparison did not yet include arbitrary guest-stack RAM, allowing the asymmetric side effect to
survive the register-only result check.

## What was tried / dead ends

The original instruction sequence was initially accepted because both models agreed on all 34
captured registers. That was insufficient evidence for a RAM-mutating leaf: equality at the call
boundary did not prove that the two models touched the same memory.

## Resolution

The model now saves those registers in the already-poisoned first 12 destination bytes, fills the
tail, restores the registers, and overwrites all 12 temporary bytes with `a1`. The comparator
selftest rejects the old stack-based instruction shape and requires that register preservation use
the destination rather than guest stack.
