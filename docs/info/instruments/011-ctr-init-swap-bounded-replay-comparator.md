---
id: I011
kind: instrument
status: trusted
created: 2026-08-24
---

## Instrument

CTR init-swap bounded replay comparator (`--startup-init-swap-next-call`)

## Validated by

Isolated serialized gate process 468226 on the exact hash-verified executable produced deterministic 34/34 agreement at `0x800771C4` across the caller jal island and the executable swap `0x80077CD8`, while the permanent forced `resident.gp=0` control produced the named 33/34 opposite; comparator selftest 23/23 includes the new "init-swap dispatch inputs remain initialized data before BSS" classification check and the no-stack-residue model regression. Its honest-refusal behavior was also observed live: requesting one window too far (`--startup-init-dispatch-next-call`) refuses with the oracle's reached-denominator instead of inventing a boundary.

## Known failure modes

Cannot see past the direct-jal capture limit of framework `oracle_trace`: indirect (jalr) call entries are neither captured nor counted, so windows ending on one refuse rather than compare. That refusal is the designed behaviour, not a silent pass.
