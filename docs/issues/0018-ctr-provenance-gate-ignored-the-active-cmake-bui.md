---
id: 18
title: CTR provenance gate ignored the active CMake build tree
status: resolved
symptom: A fresh exact-framework Clang build failed verification because psxport_sync checked stale build/psxport_resolved.txt instead of the active CMake binary directory
state_items: S003
tags: build,provenance,tooling,cmake
created: 2026-08-28
updated: 2026-08-28
---

## Root cause

`tools/psxport_sync.py --check` hardcoded `build/psxport_resolved.txt`, while CTR’s supported verification uses named build trees under `scratch/build/`. CMake correctly wrote the active tree’s exact framework record, but the verifier ignored it and reported an older build as current.

## Fix

The check accepts an explicit `--resolved PATH`, and CMake `verify` passes `${CMAKE_BINARY_DIR}/psxport_resolved.txt`. The default remains available for manual compatibility. A hermetic shipping-tool contract accepts a selected record matching the pin and rejects a selected record from a different commit.

## Evidence

The negative fixture reports built `22222222` versus recorded `11111111`. The selected real build record names exact `319d30b62ba6bc417bb8edb518b5eaae97750825`; the full combined Clang verifier passes the pin check and all subsequent gates.
