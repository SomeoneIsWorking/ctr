---
id: C014
kind: claim
status: holds
created: 2026-08-25
tags: ctr04,device-boundary
depends: psxport.pin, CMakeLists.txt, tools/compare_crt0_trace.py#main, game/core/crt0_port_trace.cpp#main, game/core/bootstrap_frontier.cpp#runBootstrapToSupportedFrontier
---

## Claim

CTR's generated initializer executes its measured I_MASK/I_STAT/DPCR prefix and reaches zero-fill entry 0x800777E8 with oracle-identical CPU and device state; ctr_port's supported frontier is the same proper function boundary.

## Evidence

Clang build against isolated psxport 9c2e3f1c plus the reviewed generic DPCR/device-capture slice succeeded. On SHA-256 7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838, two original and two replay oracle captures were deterministic; generated execution agreed 37/37 at pre-instruction PC 0x800777E8 with a0=8008AF98, a1=41A, ra=80077340, I_STAT=0, I_MASK=0, DPCR=33333333. Forced DPCR=33333332 produced exactly one named mismatch (36/37). ctr_port rebuilt after moving its one-shot override to 0x800777E8; no player binary was run.

## What would falsify it

Executable identity, 24-word initializer prefix, oracle DPCR/device semantics or write provenance, generated Core MMIO behavior, comparator schema, boundary fields, or product frontier changes; repeated captures differ; the forced DPCR opposite is not isolated; or ctr_port fails to build/reach exactly 0x800777E8.
