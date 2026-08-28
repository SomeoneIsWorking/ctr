---
id: 23
title: GTE chain runs; garbage a2 (0x0CC22321) faults the chain on an unmapped read
status: open
symptom: "FATAL: UNMAPPED RAM read32 @ 0x0CC22395 (= a2+0x74) from gen_func_8006ACE0, with garbage a2=0x0CC22321 and garbage ra=0xF24BCDEE"
state_items: S003,S005
tags: gte,frontier,data-fault
created: 2026-08-28
---

## Symptom

After issue 0022's dispatch-shape fixes, the serialized real-disc product executes the whole GTE
macro chain (0x8006A52C → 0x8006C948 → 0x8006ACE0 → block-slot continuations) repeatedly, then:

```
[mem:error] FATAL: UNMAPPED RAM read32 @ 0x0CC22395 (phys 0x0CC22395) — fail-fast.
  backtrace: Core::io_read <- gen_func_8006ACE0 <- rec_dispatch <- func_8006C948
             <- rec_dispatch <- gen_func_8006A52C <- rec_dispatch <- func_8006ACE0 ...
             <- func_8003CC98 (resident main resume)
```

Full register dump and guest-stack dump in `scratch/logs/altlink-live4.log` (PID 9884, headless,
`PSXPORT_VK_HEADLESS=1`). Live heap state around the fault looks healthy (t9=0x8012A2B0 walks a
real stride-12 pointer table; s5/s5-chains hold plausible structure pointers), so the garbage is
narrow: `a2` (the library's context/block pointer for this call) and `ra` (which the library
convention keeps as a parameter-block pointer — 0xF24BCDEE is not RAM).

## What is known

- The faulting read is a2+0x74; fragment [0x8006ACE0,0x8006AD6C) has NO +116(a2) load, so the
  faulting instruction is likely inside a flood-fill DUPLICATED tail executing in that C frame, or
  a2 was different at the load and changed afterwards. Pin the exact guest PC first (a diagnostic
  that names the faulting instruction is the missing instrument).
- The chain was entered from resident main's resume path (func_8003CC98 frame) — the caller that
  set a2 before dispatching 0x8006A52C has not been identified; the dispatch does not appear in the
  direct [0x8003CC98,0x8003CEB4) window, so it is inside duplicated-tail code.
- Everything past 0x8006ACE0's dispatch is NEW execution territory — no verified path is affected;
  this is a frontier, not a regression of 0022's fix.

## What is not yet known

Who sets a2 for this library entry and what value it should hold; whether the garbage comes from an
un-emitted overlay module (a module at ~0x800FExxx was live in earlier runs and is still not in
`overlay_bases` — capture bases with PSXPORT_DEBUG=cd), an unimplemented leaf returning wrong data,
or an upstream mis-emission.
