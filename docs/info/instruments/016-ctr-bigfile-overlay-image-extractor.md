---
id: I016
kind: instrument
status: trusted
created: 2026-08-28
---

## Instrument

CTR BIGFILE overlay image extractor and code/data classifier

## Validated by

The hermetic selftest passes 9/9: it accepts a well-formed synthetic index, refuses a short index, an
empty index, and overlapping entries, refuses both a non-`BF` stem and a short id, and — the point of
the check — makes the classifier answer BOTH ways, reporting code for a synthetic MIPS body and DATA
for a constant-filled image. On the real archive it reports every entry's id, sector, size, verdict
and the counts the verdict rests on, so a data module through a code slot is visible rather than
silent.

## Known failure modes

The classifier is a jr-ra/jal presence test, not a proof of pure code. The extractor covers only the
entries the title manifest names, so a module the game loads but nobody declared is not reported
here; runtime image activation must independently report an unmapped executable address.
