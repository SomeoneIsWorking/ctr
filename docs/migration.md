# Native/Lightrec migration

This is CTR's local execution migration plan. The portfolio authority is
`../../../shared/jit-common/docs/migration.md`; this document applies that contract without changing
the binary facts and live boundary recorded in `docs/re-frontier.md`.

## Product boundary

The gameplay executable contains CTR's native owners plus psxport's pinned Lightrec executor. It
maps the authenticated `SCUS_944.26` resident image and verified `BIGFILE.BIG` overlay images at
runtime. It contains no generated guest source, static dispatcher, interpreter, or engine fallback.
An interpreter may be built only as a separate diagnostic target.

psxport owns the per-`Core` Lightrec instance, architectural-state synchronization, device/HLE
callbacks, image-aware override table, original calls, bounded exits, and invalidation. CTR owns its
identity, overlay policy, native CD/DMA/frame/projection/presentation owners, title continuations,
and product composition. Lightrec owns its code cache and executable memory.

## Replace `FrameCompleted` with an executor exit

The current driver throws local `FrameCompleted` from native callbacks and catches it around static
dispatch. Throwing, `longjmp`, or any equivalent host-stack escape through JIT frames is forbidden.
Replace it at the execution boundary:

1. Each wait, service, or frame-completion callback records its exact continuation and phase in the
   existing title owner, then requests a typed exit from the psxport executor.
2. Lightrec observes the request and returns normally at a safe dispatcher boundary after
   synchronizing architectural state. The result carries a typed reason; no C++ exception crosses
   emitted host code.
3. `CtrFrameDriver::stepFrame` validates the result, restores the diagnostic attribution depth it
   captured on entry, finishes exactly one field/render-list/presentation fence, and increments its
   field counter. An unexpected normal return, stale continuation, or unknown reason aborts.
4. Focused positive/negative tests cover every current throw site, nested/repeated service cases,
   exact resumption, and the opposite answer. They exercise the shipping executor path, not a
   duplicate test implementation.

## Ordered migration

1. Consume the shared psxport executor and prove one resident native override plus original call.
   Prove overlay-image identity reuse and controlled invalidation. Product link/selector inspection
   must show nonzero Lightrec execution and no interpreter/generated corpus.
2. Replace generated dispatch and `super` calls in CTR composition with executor dispatch and scoped
   original calls. Convert `FrameCompleted` as specified above while preserving all measured
   CD/DMA/frame/projection/presentation behavior.
3. Reproduce the current live frontier from issue 0023: execute the alternate-link GTE chain and
   reach the already-corrupt pair at `0x8010B2D4` before the `0x8006AB00` read. Do not patch the GTE
   consumer or substitute a pointer. This is the first wiring discriminator only.
4. Continue through a representative interactive gameplay scenario. Compare timing, interrupts,
   CPU/memory and relevant device state against the independent oracle; exercise override/original
   calls and invalidation in positive and controlled-negative cases; qualify released hosts.
5. Only after step 4 passes, remove the static generator, corpus, dispatcher/registry, seed inputs,
   generated-body adapters, generated-symbol tests, and obsolete methodology. No compatibility mode
   or selector remains.

## Frozen static path

Do not regenerate, build, or run the static product during migration. Existing measurements remain
evidence for the dynamic product's required boundary. Reaching that boundary through Lightrec is not
representative gameplay and cannot authorize deletion.

## Enhancement ordering

After faithful representative gameplay, native rendering begins at dynamically observed pre-GTE
camera/object/material producers. Widescreen changes owned projection, viewport, scissor, and proven
horizontal culling. Temporal presentation interpolates authoritative previous/current simulation
transforms without mutating guest state.
