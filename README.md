# Crash Team Racing

Native PC port of the North American Crash Team Racing release, built on
[psxport](https://github.com/SomeoneIsWorking/psxport). The target product combines title-owned
native subsystems with psxport's pinned Lightrec executor for every retail instruction that remains
guest owned.

## Current evidence

- The USA disc and `SCUS_944.26` executable are reproducibly identified and provisioned. Exact
  hashes, PS-X EXE fields, addresses, and independent CPU boundaries are in
  `docs/re-frontier.md`.
- Existing evidence reaches resident main, state-3 frame ownership, asynchronous CD/DMA service,
  runtime-loaded `BIGFILE.BIG` overlays, the first presentation boundary, and CTR's alternate-link
  GTE library chain.
- The current live frontier is issue 0023: a render-list pair at `0x8010B2D4` is already corrupt
  before `lw v1,0x74(a2)` at `0x8006AB00` faults. This is the boundary the new executor must
  reproduce, not a reason to patch the GTE consumer.
- Static RE grounds projection publication at `0x80042910` and a lens-flare producer at
  `[0x80024C4C,0x80025138)`. This is source-boundary evidence, not native rendering.

These measurements came from the retired generated-source route and remain migration evidence only.
They do not make that route the product, prove representative gameplay, or authorize its deletion.

## Product and migration contract

The target `run.sh` path provisions and validates the user-supplied disc, builds the native/Lightrec
product, and launches it without offline guest-code emission. The gameplay binary neither links nor
selects an interpreter; a separately built interpreter may serve only as a diagnostic oracle.

The current launcher still belongs to the static pipeline and is not a supported migration command.
Do not run it, emit or compile its corpus, or gather new static-product evidence. Follow
`docs/migration.md`: integrate the per-`Core` executor, convert local `FrameCompleted` unwinding into
an explicit typed executor exit, reproduce issue 0023's frontier with nonzero Lightrec blocks, then
continue to representative interactive gameplay. Only that last gate authorizes removal of the
generator, corpus, static dispatcher, seeds, generated-symbol tests, and obsolete methodology.

Media resolution remains explicit argument, CTR/generic environment or `.env`, then one
unambiguous repository-root CHD. Incorrect or conflicting inputs refuse without replacing a valid
selection. Game media, extracted executable and overlays, traces, and runtime caches remain
untracked.

## Native enhancements

Native rendering starts from game-owned camera, transform, material, primitive, ordering/depth, and
presentation state before GTE/OT/GP0 submission. Widescreen widens owned projection and viewport
without stretching the final image. Interpolation retains authoritative previous/current simulation
transforms and decorates presentation only. These modes remain off during faithful oracle
comparison.

See `docs/project-state.md` for factual coverage, `docs/project-goals.md` for completion conditions,
`docs/codemap.md` for ownership, and `docs/re-frontier.md` for the preserved evidence chain.
