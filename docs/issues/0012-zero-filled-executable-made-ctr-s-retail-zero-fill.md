---
id: 12
title: Zero-filled executable made CTR's retail zero-fill side effect unobservable
status: resolved
symptom: The bounded replay reached zero-fill entry 0x800777E8, but executing it against the executable's existing zero bytes could not prove that its 1,050 stores occurred.
state_items: S003
tags: ctr04,oracle,replay,zero-fill,memory-evidence
created: 2026-08-26
updated: 2026-08-26
---

## Root cause

The complete destination `[0x8008AF98,0x8008C000)` is zero in the selected executable. Poisoning it
at image load is invalid because the initializer first reads `0x8008AF98` as its run-once guard and
would skip the call. Register equality after the loop therefore could not distinguish real stores
from an omitted RAM side effect.

## Resolution

The replay now checks the retail callsite/body/return bytes, redirects only that call through an
instrument prelude which writes `0xA5A5A5A5`, executes the unchanged retail body, and checks all
1,050 words before reproducing next call `0x80080260`. Generated execution independently poisons at
the same function boundary, super-calls the shipping generated body, and scans guest RAM. Two oracle
runs were deterministic and CPU/device/memory evidence agreed 41/41; forcing one residual generated
word produced exactly one named 40/41 mismatch. An initial checker falsely reported all 1,050 words
non-zero because it consumed `lw` in the PSX load-delay slot; an explicit `nop` fixed the instrument,
and the real zero-filled answer then appeared. This resolves the title-local observability design and
selftest. The earlier end-to-end numbers depended on an unlanded oracle device-capture CLI, so they
are not current landed-source frontier evidence; issue 0014 owns that dependency and exact rerun.
