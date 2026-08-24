---
id: 11
title: Oracle stopped at CTR DPCR before the initializer's next function boundary
status: resolved
symptom: The bounded replay reached initializer 0x800772E0 but oracle_trace reported unsupported hardware at DPCR 0x1F8010F0, so CPU-only capture could not honestly continue to 0x800777E8.
tags: ctr04,oracle,dpcr,device-state
created: 2026-08-25
updated: 2026-08-25
---

Root cause: the narrow oracle linked the interrupt controller but no authoritative DPCR owner, and its CPU-only boundary format could not prove device equality or distinguish reset state from actual writes. Fix: factor DPCR ownership from the vendored DMA controller with hardware reset/partial-write semantics; add sticky-taint and actual-write provenance; emit a fixed I_STAT/I_MASK/DPCR device block only at a clean capture-at boundary; compare that block separately on generated execution. Evidence: generic oracle spike 82/82, CTR comparator selftest 28/28, exact CTR differential 37/37 at pre-entry 0x800777E8, and forced DPCR mismatch 36/37 while all CPU fields remain equal. The next boundary is zero-fill body 0x800777E8, not an interior MMIO instruction.
