---
id: 8
title: CTR replay cannot cross the first indirect dispatch (jalr) at 0x800771C4
status: resolved
symptom: The startup-init-dispatch window refuses — oracle counts only direct jal boundaries and generated execution refuses guest-code routing without a GuestProgramImage.
tags: ctr,oracle,replay,bus,recomp,indirect-call
created: 2026-08-24
updated: 2026-08-24
---

## Symptom

`compare_crt0_trace.py --startup-init-dispatch-next-call` refuses in one of two named ways:

1. Oracle side: `oracle_trace --capture-call 7` does not stop at the `jalr` instruction
   `0x800771DC` or its target `0x800772E0`; it runs
   into the initializer body and exits 2 with `UNSUPPORTED HARDWARE WRITE32 at 0x1F8010F0` and
   denominator `reached 6 of 7 requested executed jal call boundaries`. Exact executable words put
   the unsupported access at `0x80077334`: `sw 0x33333333` through the initialized pointer stored
   at `0x8008C02C` (value `0x1F8010F0`, DPCR). A comparison past that
   write would invent device behaviour, so refusing is correct.
2. Generated side (mechanism probe with `--capture-target 0x800772E0`): `[dispatch:error] REFUSING
   TO ROUTE guest code: the installed GameRuntime supplies no GuestProgramImage, so MAIN's resident
   text range is unknown.` from framework `overlay_router.cpp`.

## Root cause

Two capabilities are missing, one per CPU:

- Framework `oracle_trace` records only executed **direct `jal`** boundaries (`--capture-call N`
  documents itself this way); there is no validated capture/stop-at-pc mode, so an indirect call
  entry can never be a boundary today.
- The consumer runtime `CtrRuntime` deliberately installs no `GuestProgramImage`, and guest-code
  routing requires a valid `residentText` range. All calls proven so far were direct edges resolved
  at emit time; the jalr is the first dynamic dispatch on the verified path.

## What was tried / dead ends

- Treating the jalr as call ordinal 7: measured wrong — ordinal 6 is `0x800771C4`'s own entry; the
  jalr is invisible to the counter (this produced the red gate before the window split).
- Hand-running the port tracer against the stage-A replay exe with capture target `0x800772E0`:
  routing refusal above; also not a faithfulness proof even if it routed.
- Second-stage replay resuming at `0x800771C4` would need post-memset RAM presets (the poison/model
  pair only works when the thunk is actually called) — a new `build_replay` semantics, deferred.

## Resolution

Unblock order, both measured 2026-08-24:

1. Consumer-side (no framework edit): fill `CtrRuntime`'s `GuestProgramImage` fact with the already
   measured resident text range `[0x80010000,0x8008D800)` so `overlay_router` routes MAIN-range
   jalrs; gate with a mechanism probe before any agreement claim.
2. Framework-side (needs the rule-4 process: claim, worktree, red hermetic test): add a validated
   stop/capture-at-pc mode to `oracle_trace` (e.g. `--capture-at 0xADDR`) with the same
   refuse-with-denominator discipline; alternatively formalize the second-stage-replay preset.

Until then `--startup-init-dispatch-next-call` stays in the tree as an honestly-refusing instrument;
no CMake target wires it into gates.

### Resolution (2026-08-24)
Root cause fixed on both CPUs: CtrRuntime now supplies measured residentText [0x00010000,0x0008D800), the trace installs the shipping RecompRegistry and routes jalr through rec_dispatch, and psxport oracle_trace --capture-at captures a strict pre-instruction PC boundary. On verified SCUS_944.26, repeated oracle/replay captures agreed 34/34 with generated state at 0x800772E0; forced gp=0 produced 33/34. The next honest boundary is the initializer's DPCR write at 0x80077334, not the old indirect-dispatch blockage.
