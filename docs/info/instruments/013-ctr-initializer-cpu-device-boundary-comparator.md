---
id: I013
kind: instrument
status: trusted
created: 2026-08-25
---

## Instrument

CTR initializer CPU/device boundary comparator

## Validated by

On identity-verified SCUS_944.26 with the isolated generic DPCR oracle slice, two replay oracle runs gave the same pre-instruction 0x800777E8 CPU/device boundary; generated execution agreed 37/37, while forced device:DPCR=0x33333332 produced exactly one named mismatch (36/37) with all CPU fields unchanged. Selftest 28/28 also refuses malformed masks and missing device registers.

## Known failure modes

The selected `oracle_trace` must expose the landed `--capture-devices` interface. Exact framework
commit `99a42aa3` does not, so it refuses before this comparator can reproduce the end-to-end
measurement; issue 0014 tracks availability separately from the instrument's validated behavior.
