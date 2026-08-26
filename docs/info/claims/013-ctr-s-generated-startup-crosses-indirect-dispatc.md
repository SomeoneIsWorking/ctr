---
id: C013
kind: claim
status: holds
created: 2026-08-24
tags: ctr,boot,recomp
depends: psxport.pin, tools/compare_crt0_trace.py#main, game/core/ctr_runtime.cpp#guestProgramImage, game/core/bootstrap_frontier.cpp#runBootstrapToSupportedFrontier, game/core/recomp_register.cpp#installRecompiledProgram, game/core/crt0_port_trace.cpp#main
reconfirmed: 2026-08-26
verified_at: 2026-08-26 23:56:05
---

## Claim

CTR's generated startup crosses indirect dispatcher 0x800771C4 through shipping rec_dispatch and reaches the initializer entry 0x800772E0 with oracle-identical CPU state; the ctr_port product boots the provisioned executable to that same bounded frontier.

## Evidence

Clang-built oracle_trace --capture-at 0x800772E0 passed its 12-case selftest. On identity-verified SCUS_944.26, two original and two bounded-replay oracle runs were deterministic; generated execution reached 0x800772E0 through the dispatcher jalr and agreed 34/34, with forced resident.gp=0 detected as 33/34. The Clang-built no-argument ctr_port loaded entry 0x8007793C through psxport and stopped at its one-shot 0x800772E0 override under PSXPORT_NOAUDIO=1 without opening a window.

## What would falsify it

Executable identity, resident range, dispatcher bytes/data, oracle capture semantics, generated router, or any boundary field changes; ctr_port fails to reach exactly 0x800772E0; or the forced opposite is no longer detected.

## Re-confirmed 2026-08-24 22:54:59

Reconfirmed after all product/comparator/doc edits: Clang build succeeded; exact SCUS_944.26 repeated oracle/replay capture agreed 34/34 at 0x800772E0; forced gp produced named 33/34; bounded no-window ctr_port reached the same frontier.

## Re-confirmed 2026-08-24 22:59:26

Reconfirmed against freshly emitted recompiler 2026-08-22.1 substrate after trace Game/Timing ownership fix: Clang trace build succeeded; repeated oracle/replay capture agreed 34/34 at 0x800772E0; forced gp produced named 33/34; frozen-uv prepare-only path built shipping ctr_port.

## Re-confirmed 2026-08-24 23:01:43

Final concise comparator diff reconfirmed: current recompiler 2026-08-22.1 substrate and Clang trace agree 34/34 with repeated oracle capture at 0x800772E0; forced gp produced 33/34; frozen-uv prepare-only and bounded no-window ctr_port succeeded.

## Re-confirmed 2026-08-24 23:03:27

Pinned final verification against clean psxport 9c2e3f1c: frozen-uv Clang prepare-only rebuilt ctr_port from recompiler 2026-08-22.1; the complete chained boundary target passed 34/34 at 0x800772E0 and named 33/34 forced opposite; no-window ctr_port reached the same frontier.

## Re-confirmed 2026-08-26

Operator-run exact product observation on 2026-08-26 against pinned psxport 99a42aa396eb810b7872c17bcc4610d21252a61c exited 0 immediately: ctr_port loaded identity-verified scratch/raw/ctr/SCUS_944.26 at entry 0x8007793C with load 0x80010000 and text size 0x7D800, entered the measured executable entry, serviced InitHeap(base=0x8009F700,size=0x7588FC), reached the supported frontier 0x800772E0, and explicitly reported that gameplay is not available yet. The raw operator log is gitignored scratch/logs/ctr-product-99a42aa3.log.
