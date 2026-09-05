---
id: C020
kind: claim
status: holds
created: 2026-08-28
tags: ctr,bigfile,runtime-modules,identity
depends: tools/extract_overlays.py#extract, titles/ctr/overlays.json
verified_at: 2026-09-04
---

## Claim

CTR reproducibly identifies and extracts the three observed `BIGFILE.BIG` runtime module images at
their exact archive byte sizes.

## Evidence

The complete verified archive has 608 monotonic `(sector offset, byte size)` entries. Runtime
observation associated entries 225, 226, and 233 with load addresses `0x8009F6FC`, `0x800A0CB8`,
and `0x800AB9F0`. `titles/ctr/overlays.json` records those facts. The extractor validates the
complete archive identity, the index shape, manifest-to-entry consistency, output byte size, and
code/data classification. Its negative selftest rejects a malformed index and inconsistent manifest
entry instead of producing an empty success.

This proves runtime-module identity and extraction only. It does not prove module activation,
translation, invalidation, or execution in the native/Lightrec product.

## Expires when

The selected `BIGFILE.BIG` identity, its index format, the title manifest, or extractor validation
changes; falsify if a runtime trace associates any recorded archive entry with a different load
address or byte extent.
