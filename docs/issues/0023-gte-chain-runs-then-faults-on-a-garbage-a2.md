---
id: 23
title: GTE chain reaches a corrupt descriptor pair before an unmapped read
status: open
symptom: lw v1,0x74(a2) at 0x8006AB00 reads through corrupt a2=0x0CC22321
state_items: S003,S005,S009
tags: gte,frontier,data-fault
created: 2026-08-28
updated: 2026-10-04
---

## Observed boundary

After crossing the alternate-link GTE chain repeatedly, execution faults on
`lw v1,0x74(a2)` at `0x8006AB00`. The input pair at `0x8010B2D4` is already
`0x252C0001,0x043DFFDA`; the second word leads to the invalid descriptor value observed as
`a2=0x0CC22321`. This excludes the alternate-link return itself as the immediate cause.

Resident frame owner `0x80035E70` passes the list at game-state `+0x1C94`. Its publisher
`0x8003B43C` obtains a fresh buffer from bump allocator `0x8003E874` and splices the three source
lists at `+0x1920`, `+0x1948`, and `+0x1970` through node `+8`; neither the allocator nor publisher
writes the first pair at the returned address.

A whole-run range watch attributed the reproduced eight bytes at `0x8010B2D4` to a valid 203-sector
CdRead from LBA 137170 into `[0x800C1D14,0x80127514)`. The missing transition is therefore the
render-list population after publication, not an allocator write, GTE mutation, or guessed pointer.

## What the consumer really is (2026-10-04, Ghidra on the identity-verified image)

`FUN_8006aaa8`, decompiled with a real body (177 instructions, `0x8006AAA8..0x8006AD6B`, one prologue,
one return), takes `($node, $view + 0x74)`. Its loop is a plain 8-byte node walk, and `0x8006AB00` is
its first dereference of the descriptor:

```
0x8006AB00  lw   v1, 0x74(a2)      # a2 == node[1], the descriptor base
0x8006AB04  beq  v1, v0, +8       # ... compared against the PREVIOUS node's descriptor
```

- `iVar19 = node[0]` is a handler object, not a code pointer: `0x8001C830`'s neighbourhood and
  `FUN_8006aaa8` read `+0x53`, `+0x58`, `+0x5C` off it and call `*(node[0] + 0x5C)`.
- The loop terminates on `node[0] == 0`, so the list is `[handler, descriptor]` pairs ending in a
  zero handler word.
- `node` advances through the GTE RAM block at `0x1F800000` (`+4` saved before the GTE work,
  restored after it), which is the alternate-link mechanism this issue already suspected.

So the boundary word is a **descriptor base read through `node[1]`**, and neither `node[0]` nor
`node[1]` at `0x8010B2D4` is a KSEG0 address. The three questions the issue asks are answerable
only where the list is BUILT, and the build is not the publisher.

## The publisher is an allocator call, not a producer (2026-10-04)

`FUN_8003b43c` decompiles to: nine `FUN_8003105c(pool, count, size, name)` pool registrations, then

```c
uVar1 = FUN_8003e874(uVar8, s_RENDER_BUCKET_INSTANCE_80011410);
*(undefined4 *)(*(int *)(unaff_gp + 0x340) + 0x1c94) = uVar1;
```

and the three source lists get `node[2] = node + 2`. It publishes a **freshly allocated address** and
writes nothing into it. `FUN_8003e874` is the bump allocator: `return *(iVar2 + 0x14)` after
`*(iVar2 + 0x14) += (size + 3) & ~3`. Nothing in the resident text writes the first pair of the
returned list, so the producer is retail code reached per frame from `0x80035E70` after the
publication, which is why the debug-only `RenderListBoundaryDiagnostic` reports a nonzero pair-store
count only once that path runs.

## The boundary is unreachable on the current tree (2026-10-04)

Reached through the product with nonzero translated blocks, the pair at `0x8010B2D4` reproduces
verbatim — `rw 0x8010B2D4 4` on the live control channel returns `252C0001 043DFFDA F0B23211
4DBCCEE0` — but the run **never gets there**. It does not finish field 0: `[gp+0x79C] =
[0x8008D708]` is left at 3 by the guest and never cleared, so the state-zero loop at
`0x8003C934..0x8003C95C` (`while ([0x8008D708]) FUN_8001D06C();`) spins forever. Measured on a
headless Clang/Lightrec run at `build/agent-clang`:

| measurement | value |
|---|---|
| translated blocks | 1468 |
| executed instructions after ~7 min | 46,749,221,532 |
| guest calls | 233,290,297 |
| `frame` / `total` | `0 / 0` |
| `faults` | 0 |
| libcd completions delivered | 12, then nothing |

The last recorded good run of this repository (`scratch/probe/baseline.log`, 2026-09-28) left the same
loop after **430 fields / 9.7 s**, published BF0225/BF0226/BF0233, ran to **field 29,033**, and
presented 960x720 frames at 94-99% non-black. That frontier is not reproducible; the frontier in
`docs/project-state.md` (field 16,228 at `0x8006AA80`) is stale in the other direction.

## Root cause of the stall that precedes it (2026-10-04)

`CtrRuntime` declared no `GuestCdStreamCallbackLayout`, so `cd_ready_callback_pointer()` resolved to
0 and **every** CD-completion delivery arm in the framework was switched off for this title:
`Cd::pumpStream` returns at its `!readyCallbackPointer` guard, `cd_drive_stock_read` returns at the
same guard, and `deliverCdReadyCompletionOnInterrupt` returns `NotOwned` before it ever reads the
slot. Guest RAM proved what that cost, read live over the control channel at the stall:

| address | value | what it is |
|---|---|---|
| `0x8008C41C` | `0x8001C7A4` | CTR's registered **ready** callback, installed by `0x8001C4F4` through its get/set pair at `0x800719FC` — never called |
| `0x8008C420` | `0x8001C7FC` | the sync callback, which did run (it wrote `[0x8008D708] = 3`) |
| `0x8008D708` | `3` | `[gp+0x79C]`, the CD-audio task word the boot loop polls |
| `0x8008D6B8` | `0` | `[gp+0x74C]`, the arm `0x8001CFEC` needs before it will finish the task |

The task word has exactly six clearing stores in the resident text (`0x8001C3A8`, `0x8001C4C4`,
`0x8001C544`, `0x8001C7D0`, `0x8001CF78`, `0x8001D020`, all `sw zero, 0x79C(gp)` with
`gp = 0x8008CF6C`). `0x8001C7D0` is inside the ready callback `0x8001C7A4` and `0x8001D020` is inside
the service `0x8001CFEC`, which additionally needs `[0x8008D6B8] != 0`. Ghidra's `--refs
0x8008D708` reports **twelve READ references and no WRITE** — it does not resolve gp-relative stores,
so a scan of the identity-verified image through the framework's own decoder finds all eleven sites.
Fixing the declaration is therefore the cause, not a workaround: it is the only route by which the
guest's own completion code can ever run.

## Remaining gap after the fix (framework-side, not a title change)

With the layout declared, the delivery happens exactly once:

```
[cdirq] CD data-ready -> callback 0x8001C7A4 (slot 0x8008C41C, controller status 0x22, a0=2 a1=0)
                        — 1 of 1 owed completions delivered
```

`0x8001C7A4(2)` clears the task only when `FUN_8007198C()` (`[0x8008C439]`) is `0x15` or `0x16`. It
never is, because the framework's drive posts **no CD completion for an XA continuous read's end**:
`runtime/psx/cd/cdc_native.cpp`'s `stop_continuous_read()` clears `reading`, `first_sector_pending`
and `following_sector_ready` and posts nothing, and `drive_consume_sector()` routes every XA audio
sector straight to the SPU without a data-ready — correct for hardware, but under
`DeliveryOwner::GuestInterrupt` it also switches the host pump off, so a guest that counts stream
completions receives one data-ready for the whole `CdlReadS` and never its end. Measured: the stream
started at LBA 139097 (`setloc 30:56:47`), ran to `EOF @ LBA 140038` — 941 sectors, ~6.3 s of
emulated time — and the boot loop still did not exit.

## Required resolution

Reach state 3 with the CD stream's end-of-stream completion delivered to `0x8001C7A4`, then reproduce
this boundary through the native/Lightrec product with nonzero translated blocks and all current
native owners active. Then use the debug-only `RenderListBoundaryDiagnostic` to report the three
bounded source chains, the post-publication store denominator, and the first writer. Do not patch the
GTE consumer, alter the CdRead range, or substitute an address. Reaching this boundary proves wiring
only; it does not resolve the corrupt producer or representative gameplay.
