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

- The faulting instruction is exactly `lw v1, 0x74(a2)` at `0x8006AB00`, the loop body shared by
  `0x8006AAA8` and its alternate-link continuation at `0x8006ACE0`. Ghidra and emitted code agree:
  the macro consumes consecutive `(return-or-zero, descriptor*)` pairs from `a0`; it loads the
  descriptor pointer into `a2`, then reads `a2+0x74`.
- The corrupt pair already exists at the macro boundary. The live fault had `a0=0x8010B2D4`, whose
  first two words were `0x252C0001, 0x043DFFDA`; the latter is the unmapped `a2`. This excludes an
  alternate-link return mapping as the immediate cause: the `jalr t2,v1` return reaches the correct
  loop, but its caller-supplied input list is invalid before the helper call.
- Resident frame owner `0x80035E70` reaches the macro only when game-state flags `+0x256C & 0x20`
  are set. It passes the list stored at game-state `+0x1C94` and a descriptor base at
  `*(gameState+0x10)+0x74`. The list slot has one static writer: the `0x8003B5E0` sequence in
  `0x8003B43C`, which obtains a fresh buffer from `0x8003E874` and splices nodes from the three
  state lists at `+0x1920`, `+0x1948`, and `+0x1970`.
- Static follow-up: `0x8003E874` is a bump allocator, not a constructor: it returns its current
  cursor at allocator `+0x14`, advances that cursor by the aligned request, and writes no payload.
  After `0x8003B5E0` publishes that return, `0x8003B43C` writes only the game-state slot and the
  `+8` links of the three existing source lists; it never writes either word at the returned buffer
  address. `0x8003E978` likewise records the current cursor into an allocator epoch slot rather
  than clearing the arena. Therefore a watch armed only after publication cannot identify a
  producer for the bad pair.
- Everything past 0x8006ACE0's dispatch is NEW execution territory — no verified path is affected;
  this is a frontier, not a regression of 0022's fix.

## What is not yet known

Which earlier producer owns the two words at the allocator result. The next decisive live
observation is a whole-run, word-range watch of the reproduced pair
`PSXPORT_WWATCH=8010B2D4,8010B2DC` with `PSXPORT_DEBUG=wwatch` and
`PSXPORT_WWATCH_BT=1`, armed before the first frame. It reports every writer's guest PC, registers,
and host chain; its first write to either word distinguishes stale-arena reuse from a malformed
producer. If the allocation address changes, first capture the returned `0x8003E874` value and arm
that exact eight-byte range before rerunning. Do not patch the GTE macro or substitute an address —
that would conceal the producer fault.
