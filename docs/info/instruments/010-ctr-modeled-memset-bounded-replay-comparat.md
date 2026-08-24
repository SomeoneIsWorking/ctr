---
id: I010
kind: instrument
status: trusted
created: 2026-08-24
---

## Instrument

CTR modeled-memset bounded replay comparator (`--startup-post-memset-next-call`)

## Validated by

Isolated serialized gate process 468226 on the exact hash-verified executable produced deterministic 34/34 agreement at `0x80077CD8` past the modeled A(2Bh) leaf, while the permanent forced `resident.gp=0` control produced the named 33/34 opposite. The leaf model itself is negative-proven: the replay poisons the whole `0x2584`-byte destination before entry (selftest "modeled memset poisons a BSS destination to make a missing write observable"), byte-checks and redirects the thunk to the exact injected model address (selftest "modeled memset redirects the checked thunk to its executable model"), preserves temporary registers inside that destination rather than leaving guest-stack residue (selftest "modeled memset preserves temporaries in its destination, never guest stack"; issue 9), refuses a changed thunk ("changed modeled-memset thunk refuses"), and the generated tracer aborts unless the poison is still present before it applies the fill.

## Known failure modes

(none recorded yet)
