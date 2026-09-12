# CTR RE frontier

Statuses: `re-verified` means binary/disc ground truth plus executable verification; `re-partial`
names an honest remaining gap; `todo` is not started. No hacks are tracked. Execution-method work is
ordered in `docs/migration.md`; this file retains binary and runtime observations, not an obsolete
translation workflow.

## Boot spine

### CTR-01 — Select and measure the target executable
- status: re-verified
- deps:
- evidence: C001/I001. The USA disc's `SYSTEM.CNF` names `cdrom:\SCUS_944.26;1`; `discdump list`
  reports the same root file at LBA 24 with 516,096 bytes. Its complete SHA-256 is
  `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`. The PS-X EXE entry is
  `0x8007793C`, load is `0x80010000`, text size is `0x7D800`, executable extent is
  `[0x80010000,0x8008D800)`, and the header stack is `0x801FFFF0`.
- where: `titles/ctr/README.md`; untracked extraction under `scratch/raw/ctr/`
- gap: None for executable identity. This does not prove a booting product.

### CTR-02 — Provision the selected disc and executable reproducibly
- status: re-verified
- deps: CTR-01
- evidence: C002/I002. `tools/provision.py` resolves the selected disc, extracts `SYSTEM.CNF` and
  `SCUS_944.26` transactionally, and verifies the complete recorded identity. Its selftest accepts a
  matching fixture and rejects a one-byte mutation and wrong `SYSTEM.CNF` without replacing valid
  output.
- where: `tools/provision.py`; gitignored `scratch/raw/ctr/{SYSTEM.CNF,SCUS_944.26}`
- gap: None for executable provisioning.

### CTR-03 — Establish an independent deterministic boot boundary
- status: re-verified
- deps: CTR-02
- evidence: C003/I003. A separately built Beetle/Mednafen CPU executed the selected CTR crt0 to the
  InitHeap boundary after 92,378 instructions and agreed with an independent symbolic decoder on
  7/7 comparable fields. The fixture demonstrated both an executed positive case and a named
  hardware-stop negative.
- where: recorded evidence in C003/I003 and `titles/ctr/README.md`
- gap: The independent device-aware window stops before later gameplay initialization.

### CTR-04 — Preserve the current execution frontier
- status: re-partial
- deps: CTR-03
- evidence: The deterministic comparison reached pre-instruction `0x800772E0` with 34/34 CPU fields;
  a forced `gp` mismatch produced 33/34. Binary/runtime investigation later grounded resident main
  `0x8003C58C`, state-3 frame owner `0x80035E70`, timing return `0x8003785C`, post-VSync suffix
  `0x80037880`, and frame-loop resume `0x8003CEB4`.
- where: `game/core/native_ownership.h`, current native owner modules, issues 0015 and 0019-0023
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
  - The hand-written GTE library uses `t2` as an alternate link register (`jalr t2,v1`). The reached
    chain repeatedly enters interior continuations before the current fault.
  - At the current boundary, the pair at `0x8010B2D4` is already corrupt before
    `lw v1,0x74(a2)` at `0x8006AB00`; the observed `a2` was `0x0CC22321`. Issue 0023 owns this
    falsifier. Do not patch the consumer or substitute a plausible pointer.

## Native ownership and enhancements

### CTR-05 — Identify camera state and graphics submitters
- status: re-partial
- deps: CTR-04
- evidence: C016. On identity-verified `SCUS_944.26`, the exact libgte leaves are
  `SetGeomScreen [0x8007781C,0x80077828)` and `SetGeomOffset [0x8007782C,0x80077844)`. Boot
  publishes OFX=256, OFY=120, H=320. Function `[0x80042910,0x80042974)` derives OFX/OFY/H from
  view offsets `+0x20/+0x22/+0x18` and is called at `0x80024CCC`, `0x8003BD2C`, and
  `0x8003F5C0`. Ghidra independently identified `[0x80024C4C,0x80025138)` as a lens-flare
  primitive producer and `0x80025138` as its callback registrar.
- where: `game/core/platform_hle_plan.*`, `game/video/projection_owner.*`, retained binary evidence
  in C016 and issue 0013
- gap: A serialized Lightrec capture must identify the active registration and producer callbacks,
  then follow the view object to simulation-owned camera/transforms. The other raw GTE-control writes
  remain dynamically unattributed.
- notes: The runtime's projection owner observes the measured publication through a scoped original
  call. That is not a native camera, widescreen implementation, or primitive renderer.

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
  were not verified. C019 grounds the CdRead completion ordering that unblocked the state-3 loader.
- where: current frame/service owners; future camera, transform, render queue, and native renderer
  modules under `game/`
- gap: CTR-JIT-02 must reproduce the current boundary through Lightrec. Then a live capture must
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
- where: `external/psxport/runtime/cpu/`, `game/core/{ctr_runtime,frame_driver,native_ownership}.*`,
  `tests/ctr_execution_exit.cpp`
- gap: Diagnose the next reached `budget-exhausted` exit at resident `0x8006A57C` and prove
  nested/repeated typed exits through real Lightrec execution. Other BIGFILE code images and
  real-game module replacement remain unqualified; the positive/negative replacement controls are
  asset-free. Product evidence must report nonzero translated blocks and fallback
  entries/instructions by typed reason and denominator.
- notes: No product option selects an interpreter. A backend refusal fallback must be typed, measured,
  bounded, and return to dynarec dispatch; unavailable host code generation is fatal.

### CTR-JIT-02 — Reproduce the current live frontier through Lightrec
- status: todo
- deps: CTR-JIT-01
- evidence: Issue 0023 records the current boundary: the alternate-link GTE chain executes, its input
  pair at `0x8010B2D4` is already corrupt, and `lw v1,0x74(a2)` at `0x8006AB00` faults.
- where: native/Lightrec CTR product with the current CD, DMA, frame, projection, and presentation
  owners
- gap: Reach that same boundary with nonzero Lightrec blocks, prove one native override plus scoped
  original call, and demonstrate module replacement invalidates affected blocks with positive and
  controlled-negative cases.
- notes: This is the first wiring discriminator and must not patch the GTE consumer.

### CTR-JIT-03 — Prove representative interactive gameplay
- status: todo
- deps: CTR-JIT-02
- evidence: None.
- where: CTR gameplay product plus separately built independent-oracle harness
- gap: Continue to a representative playable race with correct input, audio, presentation, timing,
  interrupts, memory, and relevant device state. Compare deterministic boundaries and qualify each
  released OS/architecture pair.
- notes: Boot, an FMV, a submitted frame, or a fallback-dominated run does not satisfy this gate.
