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
boundary is covered asset-free. The three observed BIGFILE code images now use exact read/content
identity and post-callback publication, with asset-free negative controls for changed content,
wrong destination/mode, premature publication, independent Core lifetime, and replacement. Full
module coverage, sustained product execution accounting, and frontier reproduction remain incomplete.
Issue 0024 owns this migration.

A bounded silent Linux run of the current Clang/Lightrec product against verified media completed
Vulkan device, 3D raster, headless renderer, and RmlUI initialization. The first 960x720 presentation
had 0 of 691,200 non-black pixels; this is a completed black output, not evidence of a native CTR
renderer. After fifteen libcd callback completions, retail execution failed fast at frame 14,928 on
`0x800B0B38` because no active code image owns that address. It lies within the measured BF0233
BIGFILE entry loaded at `0x800AB9F0`, reproducing issue 0019's next-stage module boundary under
Lightrec. Repeated unclaimed interrupt masks `0x200`/`0x204` did not prevent that progress and are
not this failure's immediate cause. At that earlier run, runtime block/fallback denominators,
module publication, visible gameplay, and issue 0023's later corrupt-list boundary were unverified.

After the image-owner change, a bounded silent run authenticated and published BF0225, BF0226, and
BF0233 at their exact non-overlapping extents following callbacks 13-15. Retail execution crossed
the former `0x800B0B38` strict-dispatch fault, delivered three more callbacks, and performed further
37-, 225-, and 328-sector reads. Its next stop was `budget-exhausted` at resident `0x8006A57C`
in frame 12,771. A later authenticated BF0233 retail probe showed that this was a finite guest
quantum: one unchanged runtime dispatch from the synchronized exit PC reached typed `FrameBoundary`
at `0x8003CEB4` after 110,766 cycles, 5,215 executed blocks, and 56,904 executed instructions.
The first 4,096 block entries observed advancing `t9`; 1,119 later entries were not inspected.
The title frame driver was refusing the first valid budget exit. Its shipping dispatch owner now
resumes synchronized positive-cycle budget exits within the same field, with an asset-free finite
field and zero-cycle host-loop negative passing. A corrected retail run crossed that budget stop
and next faulted at `0x8006AA80` in frame 16,228 on invalid scratchpad load/store addresses
`0x1F800938` and `0x1F80093C`. A bounded retail trace reproduced both bad
addresses in the duplicate `0x8006BF30` render path with `t3=0xFFFFFFFF`,
`at=0x1F800000`, and `a1=0x1F8007F8` while BF0233 was active. Its distinct
typed PC was `0x8006C0FC`; the second word came from live list memory at
`0x801B6070`. Authenticated control flow selects that packed stream through
the current descriptor's initial `+0xC8` pointer; the live cursor had advanced,
and its writer remains unknown.
This does not establish a visible CTR picture, sustained gameplay,
or the later issue 0023 boundary.

Gap: identify who wrote the live `0x8006B21C,0xFFFFFFFF` render-list pair and
whether that pair is valid for the authenticated primitive path,
then reach issue 0023's
corrupt render-list boundary with nonzero Lightrec execution. Representative gameplay, deterministic oracle/device comparison,
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

The production frame driver also completes a synthetic field after six ordinary budget exits with
one timing tick and one presentation fence, nonzero translated execution, and zero fallback. A
zero-cycle host-dispatch loop is refused before retry; there is no guessed turn ceiling that could
reject a long finite render list. The corrected retail product crossed the earlier budget stop and
reached the separate `0x8006AA80` fault in frame 16,228.

Gap: real-game repeated-budget execution now reaches a later fault; nested original-call exit proof
and completion beyond that fault remain missing.
