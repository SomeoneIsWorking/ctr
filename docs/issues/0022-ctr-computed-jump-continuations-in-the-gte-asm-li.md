---
id: 22
title: CTR computed-jump continuations in the GTE asm library are not recompiled
status: open
symptom: recomp-MISS for 0x8006ACE0 from 0x8006C948, which ends in `jr t2` to a caller-supplied continuation
state_items: S003,S005
tags: recompiler,computed-jump,gte,frontier
created: 2026-08-28
---

## Symptom

With the read-completion callback delivered at the right time (issue 0021), the product performs its
later module loads, presents at least 16 fields, and reaches CTR's hand-written GTE assembly library,
where it fails fast: `[recomp-MISS 0] no recompiled fn for 0x8006ACE0 (caller ra=0x800FEEA4,
a0=0x8010B264, c->pc=0x8006C948)`.

## What is known

`0x8006ACE0` is not a function entry: the word two before it is not `jr ra`, its own word decodes as
`lw s6, 0xF0(a2)`, and the executable contains no literal reference and no `lui`/`addiu` pair naming
it. It is a MID-FUNCTION continuation.

`0x8006C948` is hand-written assembly: it reads an inline constant at `ra+36`, writes three GTE
control registers, and ends in `jr t2` — it returns to a continuation the CALLER put in `t2`. No
`jal 0x8006C948` exists anywhere in the executable, so the helper is itself reached by a computed
jump. The reported `ra=0x800FEEA4` is not a return address at all: guest RAM there holds a pointer
table (`0x8006A52C`, `0x8006A8E0`, and the tags "LIDD"/"LID2"), so `ra` is stale under this control
flow.

The emitter already reports this class of blind spot on CTR: 8 of 1255 `jr $ra` sites emitted as
computed jumps, 28 unresolved `lw $ra` bases, and 13 seeded cross-boundary switch targets.

## What is not yet known

Which continuations the library's callers pass in `t2`, whether they can be enumerated statically
from the callers or must be captured from a run, and whether `main_reentry` seeds are the right
mechanism or the recompiler needs to resolve this dispatch shape itself. Nothing here claims a cause
beyond "the continuation was never emitted".
