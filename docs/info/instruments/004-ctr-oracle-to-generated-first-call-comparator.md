---
id: I004
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

CTR oracle-to-generated first-call comparator

## Validated by

On the identity-bound retail USA executable, the independent oracle supplies the first executed call
target and the generated tracer requires that target plus the header entry to exist in the shipping
registry. The comparator reported 34/34 PC/register fields equal. Its both-answer gate mutated only
the captured port-side `gp` after execution and reported exactly one disagreement (33/34); a target
absent from the generated registry refused without executing it. The parser selftest also refuses a
missing boundary block.

## Known failure modes

This instrument stops at one direct-call boundary and cannot see InitHeap/BIOS behavior or any later
hardware access. The target comes from the independent oracle trace, so a missing or malformed oracle
capture refuses rather than discovering a boundary itself. Exception unwinding stops the generated
call at the registered override and is suitable only for this one-shot harness, not a shipping frame
loop. Generated-code agreement says nothing about unexecuted emitted functions.
