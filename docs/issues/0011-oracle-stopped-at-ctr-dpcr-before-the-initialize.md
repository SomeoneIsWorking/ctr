---
id: 11
title: Oracle stopped at CTR DPCR before the initializer's next function boundary
status: resolved
symptom: The bounded replay reached initializer 0x800772E0 but oracle_trace reported unsupported hardware at DPCR 0x1F8010F0, so CPU-only capture could not honestly continue to 0x800777E8.
tags: ctr04,oracle,dpcr,device-state
created: 2026-08-25
updated: 2026-08-25
---

Root cause: the narrow oracle linked the interrupt controller but no authoritative DPCR owner, and its CPU-only boundary format could not prove device equality or distinguish reset state from actual writes. The isolated generic slice factored DPCR ownership from the vendored DMA controller with hardware reset/partial-write semantics, added sticky-taint and actual-write provenance, emitted a fixed I_STAT/I_MASK/DPCR device block only at a clean capture-at boundary, and compared that block separately on generated execution. That slice measured 82/82 in the generic oracle spike, 28/28 in the CTR comparator selftest, and 37/37 at pre-entry `0x800777E8`; forced DPCR mismatch produced 36/37 while all CPU fields remained equal. This resolves the hardware-model and comparison design, but the CLI path was not landed: exact framework commit `99a42aa3` cannot reproduce the end-to-end measurement. Issue 0014 owns that missing dependency and rerun.
