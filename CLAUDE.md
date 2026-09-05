# Crash Team Racing port

`AGENTS.md` is the repository-local instruction authority. Read it completely before work. The
product architecture and ordered migration are in `docs/migration.md`; factual coverage is in
`docs/project-state.md`; binary evidence is in `docs/re-frontier.md`.

CTR's product is the native host plus psxport's pinned Lightrec executor over authenticated
`SCUS_944.26`; activation of measured `BIGFILE.BIG` runtime images remains open. It must not expose
a standalone interpreter mode, static guest corpus, or engine selector. Only the shared backend's
typed, measured, bounded refusal fallback may execute interpreted instructions before returning to
dynarec dispatch. The static execution machinery has been deleted.

Preserve all existing title owners and measured addresses during migration. The former local
`FrameCompleted` throw/catch is replaced by a typed executor exit that returns normally through
Lightrec at a safe boundary. The title callback records the exact continuation; the frame driver
handles the exit, unwinds scoped driver/override ownership, commits one field fence, and advances its
counter. Never unwind or `longjmp` through JIT frames.

The current frontier is issue 0023's corrupt render-list pair, reached after the alternate-link GTE
chain. The first Lightrec implementation must reproduce that boundary with current CD, DMA, frame,
projection, and presentation owners active. This wiring proof is not the representative-gameplay
gate.

All picture work remains RE-driven. Native producers consume pre-GTE state; widescreen and temporal
presentation belong beside the owned camera/simulation/render boundaries, never in GTE/OT/GP0 or
framebuffer reconstruction.
