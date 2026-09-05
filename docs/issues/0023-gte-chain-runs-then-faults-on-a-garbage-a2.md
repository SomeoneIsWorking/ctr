---
id: 23
title: GTE chain reaches a corrupt descriptor pair before an unmapped read
status: open
symptom: lw v1,0x74(a2) at 0x8006AB00 reads through corrupt a2=0x0CC22321
state_items: S003,S005,S009
tags: gte,frontier,data-fault
created: 2026-08-28
updated: 2026-09-04
---

## Observed boundary

After crossing issue 0022's alternate-link chain repeatedly, execution faults on
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

## Required resolution

First reproduce this exact boundary through the native/Lightrec product with nonzero translated
blocks and all current native owners active. Then use the debug-only
`RenderListBoundaryDiagnostic` to report the three bounded source chains, the post-publication store
denominator, and the first writer. Do not patch the GTE consumer, alter the CdRead range, or
substitute an address. Reaching this boundary proves wiring only; it does not resolve the corrupt
producer or representative gameplay.
