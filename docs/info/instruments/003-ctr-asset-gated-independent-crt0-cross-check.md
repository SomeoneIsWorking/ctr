---
id: I003
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

CTR asset-gated independent crt0 cross-check

## Validated by

The oracle_spike permanent fixture planned and ran 22/22 checks across clean execution, a named GPU hardware-stop negative, instruction stepping, and mirrored RAM. On the real measured CTR executable, oracle_trace executed 92,378 instructions to InitHeap and crossvalidate_crt0.py compared 7/7 fields successfully against a code-independent symbolic decode. Known limitation: this validates the real crt0 and oracle boundary, not a generated CTR substrate or later hardware-dependent boot.

## Known failure modes

The oracle has no BIOS mapped, so the trustworthy window ends at the first BIOS call and says
nothing about later hardware-dependent boot. The cross-check cannot directly compare the addresses
of the BSS, stack-top globals, or heap base because those locations are consumed rather than retained
in boundary registers. Most importantly, there is no generated CTR substrate yet, so this instrument
cannot compare port execution and must not be cited as a PC boot result.
