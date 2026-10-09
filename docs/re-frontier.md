# CTR RE frontier

Statuses: `re-verified` means binary/disc ground truth plus executable verification; `re-partial`
names an honest remaining gap; `todo` is not started. No hacks are tracked. Execution-method work is
ordered in `docs/migration.md`; this file retains binary and runtime observations, not an obsolete
translation workflow.

## Boot spine

### CTR-01 — Select and measure the target executable
- status: re-verified
- deps:
- evidence: The USA disc's `SYSTEM.CNF` names `cdrom:\SCUS_944.26;1`; `discdump list`
  reports the same root file at LBA 24 with 516,096 bytes. Its complete SHA-256 is
  `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`. The PS-X EXE entry is
  `0x8007793C`, load is `0x80010000`, text size is `0x7D800`, executable extent is
  `[0x80010000,0x8008D800)`, and the header stack is `0x801FFFF0`.
- where: `titles/ctr/README.md`; untracked extraction under `scratch/raw/ctr/`
- gap: None for executable identity. This does not prove a booting product.

### CTR-02 — Provision the selected disc and executable reproducibly
- status: re-verified
- deps: CTR-01
- evidence: `tools/provision.py` resolves the selected disc, extracts `SYSTEM.CNF` and
  `SCUS_944.26` transactionally, and verifies the complete recorded identity. Its selftest accepts a
  matching fixture and rejects a one-byte mutation and wrong `SYSTEM.CNF` without replacing valid
  output.
- where: `tools/provision.py`; gitignored `scratch/raw/ctr/{SYSTEM.CNF,SCUS_944.26}`
- gap: None for executable provisioning.

### CTR-03 — Establish an independent deterministic boot boundary
- status: re-verified
- deps: CTR-02
- evidence: A separately built Beetle/Mednafen CPU executed the selected CTR crt0 to the
  InitHeap boundary after 92,378 instructions and agreed with an independent symbolic decoder on
  7/7 comparable fields. The fixture demonstrated both an executed positive case and a named
  hardware-stop negative.
- where: recorded evidence in `titles/ctr/README.md`
- gap: The independent device-aware window stops before later gameplay initialization.

### CTR-04 — Preserve the current execution frontier
- status: re-partial
- deps: CTR-03
- evidence: The deterministic comparison reached pre-instruction `0x800772E0` with 34/34 CPU fields;
  a forced `gp` mismatch produced 33/34. Binary/runtime investigation later grounded resident main
  `0x8003C58C`, state-3 frame owner `0x80035E70`, timing return `0x8003785C`, post-VSync suffix
  `0x80037880`, and frame-loop resume `0x8003CEB4`.
- where: `game/title/native_ownership.h`, current native owner modules, and issues 0023 and 0024
- gap: Reproduce the complete observed boundary through the native/Lightrec product. New comparison
  evidence comes from Lightrec plus an independent emulator/hardware oracle or direct binary
  analysis.
- notes: Preserved observations establish these dependency facts:
  - CTR's state-zero resource path contains asynchronous waits that must yield to the native field/CD
    service rather than starve it inside one guest dispatch.
  - DMA4 completion is safely delivered after B0:17 unwinds, at the measured loop seam
    `0x8003C94C`, preserving DICR, IRQ, CPU, and chained-transfer ordering.
  - The CdRead callback registered through slot `0x8008AD10` must be delivered after the caller has
    published the queue buffer; early delivery skips the module relocation pass.
  - `BIGFILE.BIG` has a 608-entry monotonic index and begins at disc LBA 276. In a current
    Clang/Lightrec run, native CdRead completed entry 225 (2 sectors from LBA 53690 to
    `0x8009F6FC`), 226 (22 from 53692 to `0x800A0CB8`), and 233 (28 from 53842 to
    `0x800AB9F0`); each was followed by retail callback `0x80032110`. The next call to
    `0x800B0B38`, inside entry 233, failed because no active code image owned it. These LBAs
    equal the verified archive's entry offsets plus its observed disc base; the unclaimed
    `0x200`/`0x204` IRQ warnings did not stop progress to this boundary. Image extents must use the
    archive's exact 2,828/44,216/56,844-byte entry sizes, not CdRead's padded 2/22/28 sectors;
    padding entry 226 would overlap entry 233's base.
  - Measured 2026-10-08: alternates share a RAM slot (226-229 at `0x800A0CB8`, 230-233 at `0x800AB9F0`).
    BF0229 (LBA 53753, 34,388 B), BF0230 (53770, 44,612 B) and BF0231 (53792, 60,248 B) are listed in
    `titles/ctr/overlays.json`; 227, 228 and 232 are measured but not loaded by boot.
  - The hand-written GTE library uses `t2` as an alternate link register (`jalr t2,v1`). The reached
    chain repeatedly enters interior continuations before the current fault.
  - At the current boundary, the pair at `0x8010B2D4` is already corrupt before
    `lw v1,0x74(a2)` at `0x8006AB00`; the observed `a2` was `0x0CC22321`. Issue 0023 owns this
    falsifier. Do not patch the consumer or substitute a plausible pointer.
  - **EXTENDED 2026-10-04.** CTR's resident text runs at `gp = 0x8008CF6C` (measured as `$r28` in
    every observed guest frame), and its CD bookkeeping is gp-relative. Ghidra's `--refs` does not
    resolve gp-relative stores, so it reported twelve READ references and no WRITE for
    `[0x8008D708]` while a scan of the identity-verified image through the framework's own decoder
    finds eleven `sw` sites, six of which clear it with `sw zero, 0x79C(gp)`. A `--refs` answer about
    a CTR global is not a census of that global's writers.
  - **EXTENDED 2026-10-04.** The boot's CD-audio task word `[gp+0x79C] = [0x8008D708]` is cleared only
    by the registered ready callback `0x8001C7A4` (`0x8001C7D0`) or by `0x8001CFEC`, which also needs
    `[gp+0x74C] = [0x8008D6B8]` armed. That callback lives in the slot `[0x8008C41C]`, written by
    CTR's own get/set pair `0x800719FC`, and psxport only dispatches it when the runtime declares a
    `GuestCdStreamCallbackLayout`; `CtrRuntime` did not, so every CD completion arm was off for this
    title and the state-zero loop at `0x8003C934` could never leave. Its first argument is compared
    against 2 at `0x8001C7AC` (`andi $a0,$a0,0xFF; bne $a0,$v0` with `$v0 = 2`), which is why the
    declared `readyStatus` is 2 and not libcd's default 1.

## Native ownership and enhancements

### CTR-05 — Identify camera state and graphics submitters
- status: re-partial
- deps: CTR-04
- evidence: On identity-verified `SCUS_944.26`, the exact libgte leaves are
  `SetGeomScreen [0x8007781C,0x80077828)` and `SetGeomOffset [0x8007782C,0x80077844)`. Boot
  publishes OFX=256, OFY=120, H=320. Function `[0x80042910,0x80042974)` derives OFX/OFY/H from
  view offsets `+0x20/+0x22/+0x18` and is called at `0x80024CCC`, `0x8003BD2C`, and
  `0x8003F5C0`. Ghidra independently identified `[0x80024C4C,0x80025138)` as a lens-flare
  primitive producer and `0x80025138` as its callback registrar.
- where: `game/execution/platform_hle_plan.*`, `game/video/projection_owner.*`, `game/video/geometry_projection_owner.*`,
  and the recorded binary evidence in this entry
- gap: Follow the view object to simulation-owned camera/transforms, and attribute the 2D layers
  that bound the drawn band (see below). The ten geometry submitters are attributed; their packet-build
  step is still retail code.
- notes: The runtime's projection owner observes the measured publication through a scoped original
  call. That is not a native camera, widescreen implementation, or primitive renderer.
- notes: **EXTENDED 2026-09-28.** The `H` census on the identity-verified image is complete:
  **18 writers** (16 raw `ctc2 rX,$26` words plus 2
  `jal SetGeomScreen` calls) and **4 readers** (`cfc2 rX,$26` at 0x8006A6B8, 0x80070AF8, 0x80070EE8,
  0x80071150). Ten of the writers are the geometry submitters' own publication, immediately before
  their own RTPS:

  | entry | H write | direct `jal` | 0 `j` targets |
  |---|---|---|---|
  | 0x80069FFC | 0x8006A0E0 | 1 | 0 |
  | 0x8006AAA8 | 0x8006AB38 | 1 | 0 |
  | 0x8006DC30 | 0x8006DCE4 | 1 | 0 |
  | 0x8006E26C | 0x8006E300 | 1 | 0 |
  | 0x8006E588 | 0x8006EB24 | 3 | 0 |
  | 0x8006F004 | 0x8006F5B0 | 3 | 0 |
  | 0x8006F9A8 | 0x8006FA68 | 1 | 0 |
  | 0x8006FE70 | 0x8006FF28 | 2 | 0 |
  | 0x80070388 | 0x80070428 | 2 | 0 |
  | 0x80070950 | 0x800709CC | 2 | 0 |

  All ten carry the same 5-instruction tail verbatim (`sll 15`, `sll 15`, `ctc2 $24`, `ctc2 $25`,
  `ctc2 $26`) and read descriptor displacements `+0x18/+0x20/+0x22` at 10 of 10 sites. **The previous
  recorded claim that nothing in the text reads `H` back was FALSE**; it is corrected in
  `game/video/widescreen_owner.h`. The reader census is what makes the remaining widening gap
  explicable: the guest derives its own viewport extents from `H`, so a widened focal length moves the
  projection and the viewport together.

  **WHAT WOULD SETTLE THE COUPLING, named because "not determined" is only a result if the next step
  is concrete:** whether the four `H` readers are proportional (a wider `H` widens the derived
  viewport by the same ratio, benign) or absolute (a gameplay-visible bound). One run, reading
  `[scene+0xF4]` — the word 0x8006A6B8 writes as `2H - CR7` — in the 4:3 and 16:9 legs and comparing
  it against the 684/512 ratio answers it, and the same run's prim census (`bg` column) names which
  2D prims produce the last non-black column that bounds the drawn band.

### CTR-06 — Native widescreen
- status: todo
- deps: CTR-05, CTR-08
- evidence: Not started.
- where: future native camera and render producers
- gap: Enable widescreen only after CTR-08 owns the frame's camera/projection and display-list
  producers, then change projection, viewport, scissor, and proven horizontal culling. Do not stretch
  the guest image or patch OFX/H constants.

### CTR-07 — Transform interpolation
- status: todo
- deps: CTR-05, CTR-08
- evidence: Not started.
- where: future PC-owned transform producers
- gap: The port has no native simulation owner or previous/current camera and object-transform
  history. A temporal presentation decorator may interpolate those authoritative states only after
  their owners exist.

### CTR-08 — Reach a visible frame and own a native primitive renderer
- status: re-partial
- deps: CTR-JIT-02, CTR-05
- evidence: The exact frame and service addresses are retained in `native_ownership.h`; native CD,
  DMA, frame, projection, and presentation owners are composed around them. An earlier observed run
  initialized the Vulkan GTE presenter and submitted a 960x720 image, but pixels and sustained cadence
  were not verified. The CdRead completion ordering measured at
  `game/disc/async_disc_owner.cpp` unblocked the state-3 loader.
- where: current frame/service owners; future camera, transform, render queue, and native renderer
  modules under `game/`
- gap: CTR-JIT-02 must reproduce the preserved boundary through Lightrec. Then a live capture must
  identify active producers and sustained cadence before one producer is replaced by a native
  override and compared through scoped original execution. Guest GTE/OT/GP0/framebuffer data remains
  diagnostic evidence, never native renderer input.

## Runtime execution migration

### CTR-JIT-01 — Connect Lightrec and typed frame exits after static removal
- status: re-partial
- deps: CTR-02, CTR-03, CTR-04
- evidence: The static execution machinery is absent. CTR builds against psxport's per-`Core`
  Lightrec executor and uses image-aware dispatch/original-call APIs. Native callbacks request
  `ExecutionExitReason::FrameBoundary` and return normally; an asset-free focused test preserves a
  complete typed result through the production propagation seam. Exact BF0225/BF0226/BF0233 reads
  were authenticated against the verified archive and published after their retail callbacks in a
  current Lightrec run, which crossed the former `0x800B0B38` image-identity stop.
- where: `external/psxport/runtime/cpu/`, `game/{entry/ctr_runtime,frame/frame_driver,title/native_ownership}.*`,
  `tests/ctr_execution_exit.cpp`
- gap: The `0x8006A57C` `budget-exhausted` exit was diagnosed as a finite guest quantum (110,766
  cycles, 5,215 blocks, 56,904 instructions from the synchronized exit PC to a typed
  `FrameBoundary` at `0x8003CEB4`), and the frame driver now resumes a synchronized positive-cycle
  budget exit inside the same field. The next live stop is `0x8006AA80` in field 16,228 on invalid
  scratchpad addresses `0x1F800938`/`0x1F80093C`, reproduced in the duplicate `0x8006BF30` render
  path with `t3=0xFFFFFFFF`. Still to prove: nested/repeated typed exits through real Lightrec
  execution beyond that fault, other BIGFILE code images, and real-game module replacement. The
  positive/negative replacement controls are asset-free. Product evidence must report nonzero
  translated blocks and fallback entries/instructions by typed reason and denominator.
- notes: No product option selects an interpreter. A backend refusal fallback must be typed, measured,
  bounded, and return to dynarec dispatch; unavailable host code generation is fatal.

### CTR-JIT-02 — Reproduce the current live frontier through Lightrec
- status: todo
- deps: CTR-JIT-01
- evidence: Issue 0023 records the preserved boundary: the alternate-link GTE chain executes, its input
  pair at `0x8010B2D4` is already corrupt, and `lw v1,0x74(a2)` at `0x8006AB00` faults. Measured
  2026-10-04: that pair reproduces verbatim on the live product (`rw 0x8010B2D4 4` →
  `252C0001 043DFFDA F0B23211 4DBCCEE0`) but the run never reaches it, because boot does not leave
  the state-zero CD-audio wait at `0x8003C934` (`frame=0`, 233,290,297 guest calls, 46,749,221,532
  instructions, `faults=0`, 12 libcd completions). `FUN_8006aaa8` is decompiled with a real body and
  is a plain 8-byte `[handler, descriptor]` node walk whose `a2` is `node[1]`; `FUN_8003b43c` only
  publishes a fresh bump-allocator address at game-state `+0x1C94`, so the pair's producer is the
  per-frame retail path, not the publisher.
- where: native/Lightrec CTR product with the current CD, DMA, frame, projection, and presentation
  owners; `game/entry/ctr_runtime.*`, `game/title/native_ownership.h`
- gap: Reach state 3 with the CD stream's end-of-stream completion delivered to the guest's registered
  ready callback `[0x8008C41C] = 0x8001C7A4`, then reproduce the boundary with nonzero Lightrec blocks,
  prove one native override plus a scoped original call, and demonstrate module replacement
  invalidates affected blocks with positive and controlled-negative cases.
- notes: This is the first wiring discriminator and must not patch the GTE consumer. The boot stall is
  NOT the recorded `0x8006AA80` fault: the 2026-09-28 run in `scratch/probe/` left the same wait after
  430 fields, published BF0225/226/233 and reached field 29,033 with presented 960x720 frames, so both
  recorded frontiers predate the current tree.

### CTR-REC-01 — Scene cuts for the Record path
- status: partial
- deps: CTR-05
- evidence: Decompiled with `decomp_pipeline.py --image-name ctr` (the run's audit reported "no pre-script record", so
  treat the C as unaudited). `FUN_8003C58C` is the main loop: `switch([gp+0x188])` with 0 = init, 1 = level
  load, 2 = unload, 3 = run, 4 = shutdown. `gp = 0x8008CF6C`; `[gp+0x340]` (`0x8008D2AC`) holds the game tracker
  pointer. In state 3, `[gp+0x18C]` is the load request (-1 running, -4/-5/-6 load kinds, set by
  `FUN_8003CFC0` with the target level in `[gp+0x190]`); `tracker+0x1A10` is the level id (`FUN_80033610` indexes
  the level table with it; 0x29 and 0x27/0x40 are tested as menu/overlay levels); `tracker+0` bit
  `0x40000000` is set when state 3 starts or a request is pending, cleared on the 1/2 transitions, and gates the
  gameplay update `FUN_80034D54` in state 3.
- where: `game/video/scene_cut.*`, `CtrRuntime::sealedFrameIsCut`
- gap: Race start (the flyby-to-race camera switch) has no identified guest variable yet, so it is not declared;
  nothing here has been checked against the cut values on the product yet (the product now presents; issue 0033).
- notes: Unit-tested through `PresentationOwner::finishField` in `tests/ctr_field_owners.cpp`.

### CTR-JIT-03 — Prove representative interactive gameplay
- status: re-partial
- deps: CTR-JIT-02
- evidence: 2026-10-09: title Start, Arcade, Single, 1P, Easy, Crash, Crash Cove reaches a race and the kart moves under pad
  input. Driver array at `gGT+0x24EC` (`gGT = 0x80096B20`, the pointer at `gp+0x340`); `driver+0x2D4/0x2D8/0x2DC` are x/y/z in
  1/256 units. The race clock `gGT+0x1D10` is in 1/960 s and `gGT+0x1D04` is the frame's dt. `FUN_80034D54` derives dt from
  `FUN_8004B41C` (root counter 1 through `FUN_80034AA4`, `gp+0xA1C`); the retail VSync callback `FUN_80034AA4` is the vblank callback libetc's ISR
  `FUN_8007C8D8` runs (it counts at `0x8008C754`); `[gp+0x348]` is its two-field countdown. Issues 0035, 0036, 0037.
- where: `replays/gameplay/arcade-crash-cove-drive.pad`, `game/execution/platform_hle_plan.cpp`, psxport `Timing::hSyncCounterWrite`
- gap: audio, a lap, and a deterministic-boundary comparison against an independent oracle are open.
- notes: Boot, an FMV, a submitted frame, or a fallback-dominated run does not satisfy this gate.
