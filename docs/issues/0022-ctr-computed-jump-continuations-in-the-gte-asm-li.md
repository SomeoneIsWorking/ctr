---
id: 22
title: CTR computed-jump continuations in the GTE asm library are not recompiled
status: resolved
symptom: recomp-MISS for 0x8006ACE0 from 0x8006C948, which ends in `jr t2` to a caller-supplied continuation
state_items: S003,S005
tags: recompiler,computed-jump,gte,frontier
created: 2026-08-28
resolved: 2026-08-28
---

## Symptom

With the read-completion callback delivered at the right time (issue 0021), the product performs its
later module loads, presents at least 16 fields, and reaches CTR's hand-written GTE assembly library,
where it fails fast: `[recomp-MISS 0] no recompiled fn for 0x8006ACE0 (caller ra=0x800FEEA4,
a0=0x8010B264, c->pc=0x8006C948)`.

## Root cause — measured, not inferred

The library uses an ALTERNATE LINK REGISTER convention, in two nested layers:

1. **`t2` is a return-address register.** All 53 `jr t2` sites in the resident executable have NO t2
   definition inside their own body (measured: backward windows over every site) — t2 is always
   caller-supplied. `jalr t2, v1` at 0x8006ACD8 is a normal CALL with its return address
   (0x8006ACE0, the instruction after its delay slot) in t2, because $ra is reserved for the
   caller's inline parameter block. The helper returns `jr t2`. The emitter already wrote
   `c->r[10] = 0x8006ACE0u` at the call site and routed `jr t2` through the dispatcher — the
   continuation just was never a dispatchable ENTRY.

2. The reported `ra=0x800FEEA4` was never a return address: it is the parameter-block pointer, and
   the blocks (244-byte stride, e.g. 0x800FEAA8+) are HEAP data built at runtime by init code —
   `lui/ori` pairs (0x80071484/0x8007148C construct 0x8006A8E0) stored with `sw t0, 0xF0(fp)`, then
   dispatched by `lw s6, 0xF0(a2)` + `jalr ra, s6` (0x8006A534). The blocks hold NINE distinct
   in-text library interior entries: 0x8006A52C 0x8006A8E0 0x8006AD88 0x8006B030 0x8006BF30
   0x8006C948 0x8006D428 0x8006D55C 0x8006D59C — the GTE macros share bodies with multiple interior
   entry points, picked at runtime by a flag branch inside the constructor.

## What landed

- **Framework (psxport emit.py, RECOMP_VERSION 2026-08-28.1, uncommitted — operator lands):**
  - `emit_module` now derives ALTERNATE-LINK CALL CONTINUATIONS from the binary: every `jalr rd, rs`
    with rd neither $zero nor $ra contributes its link value (addr+8) as a dispatchable re-entry.
    CTR MAIN: 35 sites → 35 continuations (34 newly seeded). rd=$zero is a tail call and rd=$ra
    returns through the C call stack, so neither contributes — both are pinned by negative controls.
  - `ra_computed_jumps` now takes the re-entry set and FORGETS the reaching state at every re-entry
    boundary: control can arrive FRESH there with the incoming caller's link in $ra, so a `jr $ra`
    downstream of a boundary must not be proven a coroutine resume from link-writes that only
    reached it through the re-entry's address span. CTR MAIN's false coroutine count dropped
    8 → 4 of 1255; the live proof is `jalr ra, s6` at 0x8006A534 + `jr $ra` at 0x8006AA94 inside
    re-entry fragment 0x8006A8E0, which recomp-MISSed on the incoming link 0x8006A53C before the fix.
  - Red/green: `test_jalr_alternate_link_continuation_is_dispatchable` (positive + two negatives)
    and `test_reentry_boundary_forgets_ra_for_the_coroutine_proof` (shown RED by disabling the
    merge). Suite 61/61, decoder 9/9, framework ctest 119/119.
- **Game (`game/recomp_seeds.json`):** 0x8006A8E0 and 0x8006BF30 added to `main_reentry` with
  provenance. These are the two of the nine block values that no scan seeds (the constructor-pointer
  scan correctly declines them: it cannot link a store site to a dispatch site across fragments —
  Vagrant issue #23's open problem). The designed path for fn-pointer-reached entries no scan sees
  is the seed file; the live [recomp-MISS] named them.

## Result

The product executes the whole GTE macro chain end-to-end and repeatedly
(0x8006A52C → 0x8006C948 → 0x8006ACE0 → continuations, across repeated traversals in one run) and
fails fast LATER on a guest data fault inside the chain — `UNMAPPED RAM read32 @ 0x0CC22395`
(= a2+0x74 with garbage a2=0x0CC22321, garbage ra=0xF24BCDEE) from gen_func_8006ACE0
(scratch/logs/altlink-live4.log). Issue 0023 owns that boundary.
