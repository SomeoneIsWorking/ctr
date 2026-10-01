# Crash Team Racing port agent instructions

CTR targets one native PC gameplay product: title-owned native subsystems plus psxport's pinned
Lightrec executor for every remaining retail instruction. Read `docs/migration.md`,
`docs/project-state.md`, and `docs/codemap.md` before implementation. The workspace rules in
`../AGENTS.md` and framework-consumer rules in `external/psxport/AGENTS.md` apply.

## Execution contract

- The product maps the authenticated USA `SCUS_944.26` executable and `BIGFILE.BIG` overlay images
  directly into a per-`Core` psxport-Lightrec executor. It never emits, compiles, links, or selects
  generated guest code.
- No player-facing setting may select an interpreter. Product link and selector checks must prove
  there is no standalone interpreter mode. The shared backend may use only its typed, measured,
  bounded refusal fallback and must return to dynarec dispatch; a missing host backend is fatal.
- Native overrides are keyed by image/module generation plus guest address. Original calls bypass
  only the current override and re-enter the retail body through Lightrec. Overlay replacement and
  executable writes invalidate affected translated blocks before execution resumes.
- The static translator, generated corpus, registry, seed inputs, and static-only tools/tests are
  deleted. Do not reintroduce them; recorded boundary and binary facts remain evidence only.

## Explicit executor exits

`game/core/frame_driver.cpp` uses a typed psxport executor-exit request: the native callback records its exact title
continuation/state and asks the executor to stop; Lightrec returns normally at a safe dispatcher
boundary; `CtrFrameDriver::stepFrame` handles the result, unwinds its scoped driver/override ownership,
finishes one field/presentation fence, and updates counters. Unexpected normal return or an
unrecognized exit remains fatal. Never introduce exceptions, `longjmp`, or another host-stack escape.

Preserve the current live frontier from issue 0023: the GTE alternate-link chain executes and then
faults on the already-corrupt pair at `0x8010B2D4`, with `lw v1,0x74(a2)` at `0x8006AB00`. The first
Lightrec discriminator is to reach that same boundary with current CD/DMA/frame/projection owners
active.

Native rendering consumes pre-GTE game camera/object/material state. Widescreen modifies owned
projection/viewport/culling deterministically; interpolation uses authoritative previous/current
simulation transforms. GTE/OT/GP0/framebuffer output is diagnostic evidence only.

## Measured binary facts

`SCUS_944.26` is LBA 24, 516,096 bytes, SHA-256 `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`,
load `0x80010000`, text `[0x80010000,0x8008D800)`, header stack `0x801FFFF0`; measured values are
tabulated in `titles/ctr/README.md`. CTR runs at 2 fields per game frame (30 fps), paced by a
two-field countdown at `[gp+0x348]` drained by the vertical-blank callback — the single `VSync(2)`
in the image is a boot resource load, not the frame loop. `H` has 4 measured readers, so widening it
also moves the guest's own derived viewport.

Never commit game media, extracted executables, overlays, generated guest code, `.env`, traces, or
machine-specific paths. Runtime diagnostics use `scratch/`; compiler output uses top-level `build/`.