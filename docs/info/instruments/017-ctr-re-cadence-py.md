---
id: I017
kind: instrument
status: trusted
created: 2026-09-28
---

## Instrument

tools/re_cadence.py — CTR's per-frame field count

## Validated by

SHA-256-bound measurement of SCUS_944.26 establishing **2 fields per game frame = 30 game
frames/s**: the countdown `[gp+0x348]` has exactly 4 accesses in the text (scanned 128,512
words), is armed with the literal 2 by the frame loop, drained by exactly 1 per field by the
vertical-blank callback, and blocked on by the frame suffix. It also establishes that the map's
stated REASON was wrong: the image's single `VSync(n>=2)` site `0x8003206C` is a boot resource
load inside `FUN_80031FDC`'s `param_5 == -1` branch, and the frame loop's only VSync is
`VSync(0)`, which does not wait. Reuses `tools/ctr_binary_probe.py` for the PS-X EXE reader, the
R3000A decoder, the identity assertion and the VSync argument semantics rather than adding a
second implementation. Selftest 7/7, each negative shown red on its own subject: the arming
literal, the per-field decrement, the frame suffix's block, the boot-path `VSync(2)` site, the
frame loop's `VSync(0)` delay slot, and a **fifth** countdown access. Registered as CTest
`ctr_cadence_selftest` and `ctr_cadence`, both observed green.

## Known failure modes

The fifth-access negative needed a substitution view rather than the probe's `_CorruptedAt`,
which can only inject `0xDEADBEEF`. A poison word is not an access at all, so the census would
correctly ignore it and the negative would have passed for the wrong reason — a green check that
tests nothing. `_Patched` in `re_cadence.py` substitutes a real instruction instead.

The arming literal is at `0x80037930` and the STORE to the countdown is the NEXT instruction, at
`0x80037934`. Asserting the store sits at the literal's address is an off-by-one that refuses for
the wrong reason; the store word is now checked at its own address.

Coverage: **0 of CTR's `BIGFILE.BIG` overlays are provisioned**, and 224 `jalr` sites have no
statically resolvable target. A frame-rate change made from an overlay would not be visible to
this instrument, and the tool says so on every run. This is the stated falsifier in
`docs/issues/0030`.
