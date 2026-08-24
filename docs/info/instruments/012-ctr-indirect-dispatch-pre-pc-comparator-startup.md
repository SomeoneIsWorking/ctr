---
id: I012
kind: instrument
status: trusted
created: 2026-08-24
---

## Instrument

CTR indirect-dispatch pre-PC comparator (--startup-init-dispatch-next-call)

## Validated by

On the exact provisioned SCUS_944.26, two original oracle runs and two bounded-replay runs deterministically captured pre-instruction PC 0x800772E0; shipping rec_dispatch execution agreed on PC plus 31 mutable GPRs and lo/hi (34/34), while forced resident.gp=0 produced the named 33/34 opposite. Comparator selftest 24/24 covers strict PC-boundary parsing and metadata/register disagreement.

## Known failure modes

This boundary proves only the state before `0x800772E0`; it cannot prove the initializer body or its
device side effects. The underlying oracle capture refuses an unreachable target or a successor
reached through an unsupported hardware access instead of emitting a tainted boundary.
