# Crash Team Racing port

`AGENTS.md` is the repository-local instruction authority. Read it completely before work. The
product architecture and ordered migration are in `docs/migration.md`; factual coverage is in
`docs/project-state.md`; binary evidence is in `docs/re-frontier.md`.

CTR's product is the native host plus psxport's pinned Lightrec executor over authenticated
`SCUS_944.26` and runtime-loaded `BIGFILE.BIG` images. It must not link an interpreter, static guest
corpus, or engine fallback. Do not regenerate, build, or run the retired static product.

Preserve all existing title owners and measured addresses during migration. In particular, replace
the local `FrameCompleted` C++ throw/catch with a typed executor exit that returns normally through
Lightrec at a safe boundary. The title callback records the exact continuation; the frame driver
handles the exit, restores its diagnostic-depth invariant, commits one field fence, and advances its
counter. Never unwind or `longjmp` through JIT frames.

The current frontier is issue 0023's corrupt render-list pair, reached after the alternate-link GTE
chain. The first Lightrec implementation must reproduce that boundary with current CD, DMA, frame,
projection, and presentation owners active. This wiring proof is not the representative-gameplay
gate and does not authorize static-path deletion.

All picture work remains RE-driven. Native producers consume pre-GTE state; widescreen and temporal
presentation belong beside the owned camera/simulation/render boundaries, never in GTE/OT/GP0 or
framebuffer reconstruction.
