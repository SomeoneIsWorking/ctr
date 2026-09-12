# Project state

Factual capability coverage for the Crash Team Racing port. Epic intent lives in
`docs/project-goals.md`, migration order in `docs/migration.md`, atomic work in `docs/issues/`,
ownership in `docs/codemap.md`, and the ordered binary-evidence chain in `docs/re-frontier.md`.

## Comparison baseline

The comparison baseline is the North American retail game under an accurate vanilla PlayStation
emulator. This project separately tracks its native PC host, runtime Lightrec execution, native
renderer, widescreen, interpolation, and player setup so one implemented difference cannot hide a
missing one.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The selected USA disc and `SCUS_944.26` executable are reproducibly identified and provisioned | verified | — | G001, G004 |
| S002 | Independent CPU execution establishes deterministic retail boot boundaries | verified | S001 | G001, G004 |
| S003 | Preserved evidence reaches the current resident and live execution frontiers | partial | S001, S002 | G001, G004 |
| S004 | CTR projection and primitive-producer source boundaries are grounded in the selected executable | partial | S001 | G002, G003 |
| S005 | CTR frames are produced by a game-state native renderer | missing | S004, S009 | G001, G002, G003 |
| S006 | The native camera and projection support true widescreen | missing | S005 | G002 |
| S007 | Native camera and object transforms are interpolated for presentation | missing | S005 | G003 |
| S008 | The default CTR product reaches sustained playable gameplay with input and audio | missing | S005, S009, S010 | G001 |
| S009 | The native/Lightrec product reaches the preserved CTR frontier without a standalone interpreter mode or generated code | partial | S001, S002, S003, S010 | G001, G004 |
| S010 | CTR frame/service suspension uses explicit typed executor exits | partial | S003 | G001 |

## Current focus

S009 is the current focus. The static route and exception unwinding are already absent. Reproduce
issue 0023's live boundary through psxport's per-`Core` Lightrec executor with the existing native
owners active, then prove overlay activation, scoped original calls, invalidation, and bounded
fallback accounting on the shipping boundary.

## Hosted verification and host gaps

Hosted CI is asset-free and must not download a disc, executable, BIOS, extracted module, or runtime
trace. The Linux x86_64 job builds the actual CTR native/Lightrec product with the recorded framework
revision, runs its focused tests, and inspects the linked executable for forbidden static or
standalone-interpreter ownership. It proves compilation and composition only, not game execution.
Evidence: the Linux x86_64 asset-free composition gate passed on main commit
`3499978413daf00fd2eed95d5630c30bdb491551` in
[run 33960150550](https://github.com/SomeoneIsWorking/ctr/actions/runs/33960150550).

| Host | Hosted boundary | Current gap |
|---|---|---|
| Linux x86_64 | Native/Lightrec product build, focused tests, and linked-boundary inspection | Real CTR gameplay and oracle comparison require user media and remain local evidence |
| Windows x86_64 | No truthful title job yet | The shared PSXPort/Lightrec Windows product build and dependency contract are not complete |
| macOS arm64 | No truthful title job yet | Apple Silicon executable-memory, ABI, cache-coherency, and product build qualification are missing |
| Android arm64-v8a | No truthful title job yet | Shared Android packaging plus PSXPort/Lightrec arm64 execution and CTR touch/setup ownership are missing |

## Capability details

### S001 — reproducible retail input

Evidence: claims C001/C002 and instruments I001/I002 record `SYSTEM.CNF` selecting
`SCUS_944.26`, the 516,096-byte executable and complete SHA-256 identity, PS-X EXE fields,
transactional extraction, and positive/negative provisioner controls. `BIGFILE.BIG` identity, its
608-entry monotonic index, and the measured runtime-module entries are recorded in C020/I016. No
game bytes are tracked.

### S002 — independent boot execution

Evidence: C003/I003 record the independent Beetle/Mednafen CPU reaching the first InitHeap boundary
after 92,378 instructions and agreeing with the symbolic crt0 decoder on 7/7 comparable fields. The
oracle fixture also demonstrates a named hardware-stop result.

### S003 — preserved resident and live execution frontiers

Partial evidence: the independent comparison reaches pre-instruction `0x800772E0` with 34/34 CPU
fields and a forced 33/34 negative. Preserved observations later crossed the title-owned frame, CD,
DMA, presentation, runtime-module, and alternate-link GTE paths. Issue 0023 is the current live
boundary: the input pair at `0x8010B2D4` is already corrupt before `lw v1,0x74(a2)` at
`0x8006AB00` faults. Everything beyond the repaired alternate-link dispatch was new territory; no
earlier verified path moved.

Gap: this evidence was produced by the retired generated-source route and is frozen as the boundary
S009 must reproduce. Do not regenerate, build, run, or extend it. The independent CPU/device proof
also still ends at `0x800772E0`; previously proposed device/zero-fill extensions are not current
landed evidence. Exact addresses and controls remain in `docs/re-frontier.md`.

### S004 — projection and primitive source evidence

Partial evidence: C016 records `SetGeomScreen [0x8007781C,0x80077828)`, `SetGeomOffset
[0x8007782C,0x80077844)`, projection publication `[0x80042910,0x80042974)`, and lens-flare producer
`[0x80024C4C,0x80025138)`. The retained binary observation in `docs/re-frontier.md` records the
title's pre-GTE view publication boundary.

Gap: static identity does not establish the active camera, dynamic producer order, native primitive
ownership, or a representative visible frame. The remaining raw GTE-control writes are not
dynamically attributed.

### S005 — game-state native renderer

Missing capability: current frame work remains guest owned. There is no native camera/object
producer set, render queue, ordering/depth owner, or native renderer. The existing compatibility
presentation evidence does not satisfy this capability, and guest GTE/OT/GP0/framebuffer data is not
a permitted product input.

### S006 — true widescreen

Missing capability: no native camera/projection owner can widen view geometry. Recorded OFX/OFY/H
values are evidence only. The future implementation must change owned projection, viewport, scissor,
and proven horizontal culling without stretching the final image.

### S007 — interpolated presentation

Missing capability: no authoritative native simulation tick or consecutive native camera/object
transforms exist. Presentation therefore has no grounded state pair to interpolate without rerunning
guest code or consuming quantized output.

### S008 — playable default product

Missing capability: no native/Lightrec product reaches a representative playable race with working
input, audio, presentation, timing, and persistence. A boot boundary, first submitted image, menu,
or attract/FMV sequence cannot verify this item.

### S009 — native/Lightrec product

Partial capability: the static translator, generated corpus, registry, seed inputs, and static-only
tools/tests are absent. CTR builds against psxport's per-`Core` Lightrec executor, and composition
targets its image-aware dispatch, original-call, invalidation, and typed-exit boundaries. The linked
boundary is covered asset-free; runtime overlay activation, nonzero real-game block execution,
bounded-fallback denominators, and frontier reproduction remain incomplete. Issue 0024 owns this
migration.

A bounded twelve-second silent Linux run of the current Clang/Lightrec product against verified
media reached twelve libcd callback completions, a polled-read counter of eight, NTSC display setup,
and the first `FramePresenter::commit` call. It continued through repeated unclaimed interrupt masks
`0x200`/`0x204`; those warnings are not established as the cause of a stall. The bound interrupted
Vulkan initialization before a completed picture, gameplay, or a complete runtime counter report.

Gap: preserve all current CD, DMA, frame, projection, and presentation owners and
reach issue 0023's corrupt render-list boundary with nonzero Lightrec execution. Representative gameplay, deterministic oracle/device comparison,
override and original-call coverage, invalidation controls, and released-host qualification remain
required.

### S010 — explicit executor exits

Partial capability: `game/core/frame_driver.cpp` no longer uses `FrameCompleted` exceptions. Native
wait and completion owners record title state, request `ExecutionExitReason::FrameBoundary`, and
return normally. `CtrFrameDriver::stepFrame` validates the typed result and finishes one field. The
production propagation seam has focused coverage preserving its reason, guest PC, cycle count, and
detail. A synthetic five-field program executes through Lightrec and the production frame driver:
the timing override calls its original body, the suffix restores a different return address, each
field advances one presentation fence, and the next field starts at the exact continuation without
leaking an override or pending exit. It requires nonzero translated blocks/instructions and zero
fallback. The negative exposed the old function-dispatch assumption: field 1 stopped immediately
because its entry equaled the incoming return register. Whole-field dispatch now uses the shared
until-exit API; suffix calls name their independently known continuation. The final two fields
rewrite previously translated code through the shared native-store owner, suspend/resume the audio
wait, and cross a nested original-call/native-return boundary. They require exactly two services
and five presentation fences, exposing stale translated code and repeated native continuation
before their shared fixes.

Gap: real-game nested/repeated exit proof through Lightrec is still missing.
