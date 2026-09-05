---
id: I003
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

CTR asset-gated independent crt0 cross-check

## Validated by

The oracle fixture ran 22/22 checks across clean execution, a named GPU hardware-stop negative,
instruction stepping, and mirrored RAM. On the selected CTR executable it executed 92,378
instructions to InitHeap and compared 7/7 fields successfully against a code-independent symbolic
decode. Known limitation: this instrument validates the real crt0 and independent CPU boundary, not
later BIOS or hardware-dependent boot.

## Known failure modes

The oracle has no BIOS mapped, so the trustworthy window ends at the first BIOS call and says
nothing about later hardware-dependent boot. The cross-check cannot directly compare the addresses
of the BSS, stack-top globals, or heap base because those locations are consumed rather than retained
in boundary registers. The oracle still cannot continue through BIOS semantics by itself. The
retained measurement checks the original executable at this first-call boundary but may not be cited
as a PC boot result.
