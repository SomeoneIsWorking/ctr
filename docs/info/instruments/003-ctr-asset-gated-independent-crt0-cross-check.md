---
id: I003
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

CTR asset-gated independent crt0 cross-check

## Validated by

The oracle_spike permanent fixture planned and ran 22/22 checks across clean execution, a named GPU hardware-stop negative, instruction stepping, and mirrored RAM. On the real measured CTR executable, oracle_trace executed 92,378 instructions to InitHeap and crossvalidate_crt0.py compared 7/7 fields successfully against a code-independent symbolic decode. Known limitation: this instrument validates the real crt0 and oracle boundary, not later BIOS or hardware-dependent boot; I004 owns the separate generated-side comparison.

## Known failure modes

The oracle has no BIOS mapped, so the trustworthy window ends at the first BIOS call and says
nothing about later hardware-dependent boot. The cross-check cannot directly compare the addresses
of the BSS, stack-top globals, or heap base because those locations are consumed rather than retained
in boundary registers. The oracle still cannot continue through BIOS semantics by itself. The
separate I004 comparator now checks the generated substrate at this first-call boundary, but neither
instrument may be cited as a PC boot result.
