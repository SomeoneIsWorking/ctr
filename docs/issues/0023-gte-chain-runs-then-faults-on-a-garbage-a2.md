---
id: 23
title: GTE chain runs; garbage a2 (0x0CC22321) faults the chain on an unmapped read
status: open
symptom: "FATAL: UNMAPPED RAM read32 @ 0x0CC22395 (= a2+0x74) from gen_func_8006ACE0, with garbage a2=0x0CC22321 and garbage ra=0xF24BCDEE"
state_items: S003,S005,S009
tags: gte,frontier,data-fault
created: 2026-08-28
updated: 2026-08-31
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
- The pre-frame whole-run range watch is now decisive: `0x8010B2D4` is `0x495C0` bytes into the
  valid 203-sector `CdRead` from LBA 137170 to `0x800C1D14` (ending `0x80127514`), mode `0x80`.
  Its first eight bytes are the reproduced pair. The host chain is
  `Core::mem_w8 <- cd_read_stock_sync <- ctr::cdReadWithCompletionCallback <- func_80076F10`;
  the watcher's `pc=0x80076F10`, `ra=0x80032644`, `a0=203`, `a1=0x800C1D14`, and `a2=0x80` agree.
  The pair is raw disc payload, not an allocator write, GTE mutation, or watcher-attribution bug.

## What is not yet known

The CD stream is not on the module relocation path: its callback only completes the resource stage;
the separate module chain is `0x800321B4 -> 0x80032110 -> 0x80031D30 -> 0x800326B4`. The missing
transition is the render-list population after `0x8003B43C` allocates and publishes
`game+0x1C94 = 0x8010B2D4`: it links the source lists at `game+0x1920/+0x1948/+0x1970` through
node `+8`, but does not write the first `(return, descriptor*)` pair. The debug-only
`RenderListBoundaryDiagnostic`, enabled with `PSXPORT_DEBUG=ctr-render-list`, now preserves that
generated publisher as a super-call, reports the three bounded source chains, and arms Core's
existing write observer on the returned pair for the rest of the field. Its hermetic fixture proves
the post-publication zero-store state and records a later bounded pair write. A native/Lightrec
real-disc run must establish the actual store denominator and first writer before any GTE, CD, or
consumer change. Do not patch the
GTE macro, CdRead range, or substitute an address, because each would conceal this render-list
integration boundary.

The execution plan now requires psxport-Lightrec. Preserve this exact fault and pre-fault data as the
first dynamic-product discriminator; do not rerun the static product to extend the issue. Reaching
the same boundary through Lightrec does not resolve the corrupt producer and does not authorize
static-path deletion.
