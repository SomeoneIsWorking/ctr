# Crash Team Racing project goals

## G001 — Faithful playable PC port

Build a native PC product from the selected USA Crash Team Racing executable and user-supplied disc
content. Title-owned native subsystems cooperate with psxport's pinned Lightrec executor, which
translates every remaining retail instruction on demand. The default launcher must boot the real
game, render its frames, accept input, produce audio, and sustain gameplay. Independent reference
execution must cover the behavioral boundaries used as evidence; compilation, static discovery, or
a bounded boot probe does not satisfy this goal.

Success conditions:

- `./run.sh` provisions the required user assets and launches the native/Lightrec product without
  offline guest-code emission or maintainer-only reverse-engineering tools.
- The product reaches and sustains visible gameplay with working input and audio.
- Faithful behavior is compared against independent retail execution at deterministic boundaries,
  with every known divergence recorded rather than hidden by a fallback.
- Product link/selector checks prove that no interpreter, generated guest corpus, or engine fallback
  is present; native overrides, original calls, bounded exits, and invalidation use the shipping
  executor contract.

Constraints and non-goals:

- No build, install, provisioning, or launch step emits or compiles guest code ahead of time.
- An interpreter may be built only as a separate test/diagnostic oracle and is absent from gameplay.
- The static route is frozen migration evidence until representative gameplay authorizes complete
  removal; it is never retained as a compatibility mode.
- Disc images, extracted executable bytes, traces, and decompiler output remain outside git.
- A framework smoke target, static producer census, or pre-frame boot boundary is not the product.

## G002 — Game-state native renderer and true widescreen

Render CTR through PC-owned camera, transform, primitive, ordering/depth, and presentation owners
whose inputs come from the game's pre-GTE state. Preserve a faithful 4:3 path and extend that owned
projection to wide aspect ratios without stretching or changing vertical framing.

Success conditions:

- The native renderer reproduces the game's visible layers from producer-owned game state with
  correct ordering/depth, materials, effects, HUD, and presentation bounds.
- Wider modes reveal correctly projected horizontal content while preserving vertical framing and
  the faithful 4:3 baseline.
- Native producer coverage and first-frame execution are demonstrated against the selected retail
  executable, including negative checks that expose missing or stale ownership.

Constraints and non-goals:

- GTE SXY, ordering-table packets, GP0 streams, and framebuffer pixels may be diagnostic evidence;
  they are never native producer input or a reconstructed fallback picture.
- Widescreen is applied at the native camera/projection owner, not by patching guest OFX/H constants
  or stretching a 320x240 image.
- A statically identified producer is evidence for where ownership belongs, not proof that the
  producer executes in a visible frame.

## G003 — Interpolated presentation at 60 fps and above

Present CTR smoothly between its original simulation ticks by retaining consecutive game-owned
camera and object transforms and interpolating them only for native rendering. Simulation rate,
gameplay logic, collision, and guest memory remain faithful to the original tick sequence.

Success conditions:

- The native frame driver retains authoritative previous/current simulation snapshots and computes
  presentation transforms from an explicit interpolation alpha.
- Camera, world geometry, objects, and effects that require temporal continuity move smoothly at
  presentation rates above the original simulation rate without changing simulation outcomes.
- Interpolation-disabled output remains the faithful baseline, and alpha endpoints reproduce the
  corresponding simulation snapshots exactly.

Constraints and non-goals:

- Quantized GTE results, ordering-table output, and composed guest matrices are not interpolation
  sources.
- Frame duplication, guest re-execution under a modified camera, and simulation-rate patches do not
  satisfy interpolation.
- This goal depends on PC ownership of the transforms and native renderer that consume them.

## G004 — Reproducible evidence and player setup

Keep executable identity, runtime image mapping, framework provenance, and verification reproducible
from a fresh clone with the documented native dependencies, `uv`, a compatible C/C++ compiler, and
user-supplied game assets.

Success conditions:

- Executable selection and extraction refuse incorrect regional or mutated inputs by complete
  measured identity.
- One frozen Python environment drives provisioning, configuration, build, and checks.
- The recorded psxport pin names the exact framework revision used for product evidence.
- Maintainer-only tools such as Ghidra are unnecessary for a player build.
