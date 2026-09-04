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
| S009 | The native/Lightrec product reaches the preserved CTR frontier without interpreter or generated code | missing | S001, S002, S003, S010 | G001, G004 |
| S010 | CTR frame/service suspension uses explicit typed executor exits | missing | S003 | G001 |

## Current focus

S009 is the current focus. Integrate psxport's per-`Core` Lightrec executor, convert local
`FrameCompleted` unwinding into the explicit executor-exit contract, and reproduce issue 0023's
live boundary with the existing native owners active. This first discriminator does not authorize
deletion of the static route; representative interactive gameplay does.

## Capability details

### S001 — reproducible retail input

Evidence: claims C001/C002 and instruments I001/I002 record `SYSTEM.CNF` selecting
`SCUS_944.26`, the 516,096-byte executable and complete SHA-256 identity, PS-X EXE fields,
transactional extraction, and positive/negative provisioner controls. `BIGFILE.BIG` identity and its
608-entry monotonic index are separately recorded in C020/I016. No game bytes are tracked.

### S002 — independent boot execution

Evidence: C003/I003 record the independent Beetle/Mednafen CPU reaching the first InitHeap boundary
after 92,378 instructions and agreeing with the symbolic crt0 decoder on 7/7 comparable fields. The
oracle fixture also demonstrates a named hardware-stop result.

### S003 — preserved resident and live execution frontiers

Partial evidence: the independent comparison reaches pre-instruction `0x800772E0` with 34/34 CPU
fields and a forced 33/34 negative. Further recorded static-path runs crossed the title-owned frame,
CD, DMA, presentation, runtime-overlay, and alternate-link GTE paths. Issue 0023 is the current live
boundary: the input pair at `0x8010B2D4` is already corrupt before `lw v1,0x74(a2)` at
`0x8006AB00` faults. Everything beyond the repaired alternate-link dispatch was new territory; no
earlier verified path moved.

Gap: this evidence was produced by the retired generated-source route and is frozen as the boundary
S009 must reproduce. Do not regenerate, build, run, or extend it. The independent CPU/device proof
also still ends at `0x800772E0`; previously proposed device/zero-fill extensions are not current
landed evidence. Exact addresses and controls remain in `docs/re-frontier.md`.

### S004 — projection and primitive source evidence

Partial evidence: C016/I015 identify `SetGeomScreen [0x8007781C,0x80077828)`, `SetGeomOffset
[0x8007782C,0x80077844)`, projection publication `[0x80042910,0x80042974)`, and lens-flare producer
`[0x80024C4C,0x80025138)`. C018 records the title's pre-GTE view publication boundary.

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

Missing capability: CTR still routes ordinary retail execution and native-owner original calls
through generated host functions. It has not mapped authenticated resident and overlay images into a
per-`Core` Lightrec executor, registered overrides by image generation plus address, or proven
runtime invalidation when overlays replace executable bytes. Product link/selector proof excluding
the interpreter and generated corpus is also absent. Issue 0024 owns this migration.

First discriminator: preserve all current CD, DMA, frame, projection, and presentation owners and
reach issue 0023's corrupt render-list boundary with nonzero Lightrec execution. This wiring proof
does not authorize deletion. Representative gameplay, deterministic oracle/device comparison,
override and original-call coverage, invalidation controls, and released-host qualification remain
required.

### S010 — explicit executor exits

Missing capability: `game/core/frame_driver.cpp` uses local `FrameCompleted` exceptions to escape
generated host frames at waits, service points, and field completion. A JIT-safe design records the
exact continuation in title state, requests a typed psxport executor exit, and lets Lightrec return
normally at a safe dispatcher boundary. `CtrFrameDriver::stepFrame` then validates the reason,
restores its captured diagnostic-depth invariant, finishes exactly one field/presentation fence, and
advances counters. Unexpected normal return, stale state, and unknown reasons remain fatal; `longjmp`
or other host-stack unwinding is not an alternative.
