---
id: I006
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

CTR bounded resident replay comparator

## Validated by

Production helper selftests demonstrate deterministic replay, main-RAM alias exclusion, and
stale-prefix, missing-zero-run, and missing-scratch refusals. Two real original oracle runs and two
real replay oracle runs were deterministic; the measured USA executable produced both 34/34
agreement and a named 33/34 forced `resident.gp` disagreement through canonical `--capture-call 1`.

## Known failure modes

The replay does not reconstruct post-crt0 RAM. It is trustworthy only through an exact validated
prefix independently proven to contain no memory reads before capture; a changed prefix, unavailable
aligned zero run, missing zero-valued scratch register, or nondeterministic repeat run must refuse.
