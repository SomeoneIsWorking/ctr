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

These measurements came from the removed generated-source route and remain evidence only. They do
not make that route the product or prove representative gameplay.

## Product and migration contract

The target `run.sh` path provisions and validates the user-supplied disc, builds the native/Lightrec
product, and launches it without offline guest-code emission. No product option selects an
interpreter. Lightrec may use only its typed, measured, bounded refusal fallback before returning to
dynarec dispatch; a separately built interpreter-only target may serve as a diagnostic oracle.

The launcher now targets the native/Lightrec composition. The translator, corpus, static dispatcher,
seed inputs, generated-symbol tests, and exception-based `FrameCompleted` control flow were removed
before integration. Follow `docs/migration.md`: finish the per-`Core` executor, reproduce issue
0023's frontier with nonzero Lightrec blocks, then continue to representative interactive gameplay.

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

## Verification

Run `uv run --frozen python tools/verify.py` for the asset-free Clang/Ninja product gate. The
verifier uses PSXPort's shared dependency, build, test, and linked-execution checks, plus CTR's
provisioning and native-owner tests. `psxport.pin` records the verified framework revision. Lightrec
and GNU Lightning are exact dependencies owned by that framework; custom checkout/prefix paths
use its `PSXPORT_LIGHTREC_DIR` and `PSXPORT_LIGHTNING_PREFIX` configuration.

Hosted Linux x86-64 runs this same command without game files. The Windows, Apple Silicon, and
Android execution/packaging gaps are recorded in `docs/project-state.md`; the current CI run does
not qualify those hosts or prove real-title gameplay.
