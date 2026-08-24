---
id: 10
title: Regenerated CTR trace segfaults after instruction timing emission
status: resolved
symptom: ctr_crt0_port_trace exits -11 immediately after loading the resident replay when current generated shards call rec_guest_instruction_ticks
tags: ctr,recomp,timing,harness
created: 2026-08-24
updated: 2026-08-24
---

## Root cause

The trace constructed `Core` directly, leaving `Core::game` null. Recompiler 2026-08-22.1 emits `rec_guest_instruction_ticks` in generated functions; that shipping hook calls `core->game->timing.advanceGuestInstructionTicks`, so the first tick dereferenced null. Older generated shards did not expose the missing ownership.

## Resolution

Construct the production `Game` owner after installing `CtrRuntime`, then use `game->core`. This initializes the same Core/Game/Timing relationship as the product without a test-only timing replacement. Rebuilt current generated trace with Clang; the real indirect-dispatch comparison returned 34/34 and forced gp returned 33/34.
