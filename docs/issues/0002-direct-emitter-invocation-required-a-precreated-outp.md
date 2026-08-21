---
id: 2
title: Direct emitter invocation required a precreated output directory
status: resolved
symptom: The first shipping-emitter command failed with FileNotFoundError for generated/rec_decls.h in a clean scaffold.
tags: workflow,recompiler,ctr-04
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

CTR had never emitted a substrate, so `generated/` did not exist. The framework emitter writes files
under the output path but does not own creation of the consumer's output directory.

## Resolution

`tools/emit_substrate.py` is now CTR's one emission entry point. It verifies the executable through
the existing provisioner authority, creates the gitignored output directory, and then invokes the
shipping emitter with the tracked empty seed manifest. A repeat run completed and emitted the same
1,236-function, eight-shard substrate.
