# Native/Lightrec migration

This is CTR's local execution migration plan. Binary facts and the live boundary remain in
`docs/re-frontier.md`; psxport owns the shared runtime contract.

## Product boundary

The gameplay executable contains CTR's native owners plus psxport's pinned Lightrec executor. It
maps the authenticated `SCUS_944.26` resident image and verified `BIGFILE.BIG` overlay images at
runtime. It contains no offline source corpus, static dispatcher, standalone interpreter mode, or
engine selector. Lightrec may use only its typed, measured, bounded refusal fallback and must return
to dynarec dispatch; an interpreter-only mode may be built only as a separate diagnostic target.

WebAssembly is part of the migration release contract: the browser-capable build must use the
same dynarec-first execution path and the same typed exit/fallback accounting as desktop. If web
parity is not yet proven, this remains a blocked milestone with explicit criteria before release.

psxport owns the per-`Core` Lightrec instance, architectural-state synchronization, device/HLE
callbacks, image-aware override table, original calls, bounded exits, and invalidation. CTR owns its
identity, overlay policy, native CD/DMA/frame/projection/presentation owners, title continuations,
and product composition. Lightrec owns its code cache and executable memory.

## Typed executor exit

The driver no longer throws `FrameCompleted`. Throwing, `longjmp`, or any equivalent host-stack
escape through JIT frames is forbidden. The execution boundary is:

1. Each wait, service, or frame-completion callback records its exact continuation and phase in the
   existing title owner, then requests a typed exit from the psxport executor.
2. Lightrec observes the request and returns normally at a safe dispatcher boundary after
   synchronizing architectural state. The result carries a typed reason; no C++ exception crosses
   emitted host code.
3. `CtrFrameDriver::stepFrame` validates the result, lets scoped driver/override ownership unwind
   normally, finishes exactly one field/render-list/presentation fence, and increments its
   field counter. An unexpected normal return, stale continuation, or unknown reason aborts.
4. Required focused positive/negative coverage includes every former throw site, nested/repeated service cases,
   exact resumption, and the opposite answer. They exercise the shipping executor path, not a
   duplicate test implementation.

Whole-field dispatch uses PSXPort's `dispatchGuestUntilExit`, which continues after supported HLE,
pending work, and syscalls while preserving typed exit/fault/budget results. A function's incoming
`ra` cannot delimit a field. Interior suffixes that restore another caller instead pass their
measured continuation explicitly to `executeFunction`; resource waits obtain it from their owned
saved frame. Native helper calls that must return normally continue to use `dispatchToReturn`.

## Ordered migration

1. Completed break-first: remove the translator, generated corpus, registry, seed inputs, static-only
   tools/tests, and obsolete methodology without building or running the old product.
2. In progress: consume the shared psxport executor and prove one resident native override plus
   original call. Prove overlay-image identity reuse and controlled invalidation. Product
   link/selector inspection must show nonzero Lightrec execution, no standalone interpreter mode,
   and no generated corpus.
3. Completed structurally: CTR composition uses executor dispatch and scoped original calls, and the
   former `FrameCompleted` sites use typed exits while preserving measured
   CD/DMA/frame/projection/presentation ownership. Real-game nested and repeated exit evidence is
   still part of step 2.
4. Reproduce the current live frontier from issue 0023: execute the alternate-link GTE chain and
   reach the already-corrupt pair at `0x8010B2D4` before the `0x8006AB00` read. Do not patch the GTE
   consumer or substitute a pointer. This is the first wiring discriminator only.
5. Continue through a representative interactive gameplay scenario. Compare timing, interrupts,
   CPU/memory and relevant device state against the independent oracle; exercise override/original
   calls and invalidation in positive and controlled-negative cases; qualify released hosts.
## Preserved evidence

Measurements from the removed route remain evidence for the dynamic product's required boundary.
They are not executable inputs, compatibility machinery, or permission to restore static translation.
Reaching that boundary through Lightrec is not representative gameplay.

## Enhancement ordering

After faithful representative gameplay, native rendering begins at dynamically observed pre-GTE
camera/object/material producers. Widescreen changes owned projection, viewport, scissor, and proven
horizontal culling. Temporal presentation interpolates authoritative previous/current simulation
transforms without mutating guest state.
