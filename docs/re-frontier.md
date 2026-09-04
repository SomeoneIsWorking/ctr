# CTR RE frontier

Statuses: `re-verified` means binary/disc ground truth plus executable verification; `re-partial`
names an honest remaining gap; `todo` is not started. No hacks are tracked.

The execution-method migration is ordered in `docs/migration.md`. Static-path observations below
are frozen evidence for the boundary Lightrec must reproduce; they are not instructions to emit,
build, or run that route, and they do not make it a product fallback.

## Boot spine

### CTR-01 — Select and measure the target executable
- status: re-verified
- deps:
- evidence: C001/I001. The USA disc's `SYSTEM.CNF` names `cdrom:\SCUS_944.26;1`; `discdump list` reports the same root file at LBA 24 with 516,096 bytes. A fresh extraction has SHA-256 `7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`, matching two older workspace extractions byte-for-byte. The Clang-built shipping `crt0_extract` reports PS-X EXE entry `0x8007793C`, load `0x80010000`, text size `0x7D800`, extent `[0x80010000,0x8008D800)`, and a COMPLETE 8-of-8 crt0 group. Its selftest exercised 59 checks including valid shapes, wrong magic, short input, an out-of-image entry, and an incomplete zeroed prologue.
- where: `titles/ctr/README.md`; untracked extraction under `scratch/raw/ctr/`
- gap: None for executable identity. This does not prove a generated substrate or a booting port.
- notes: All disc-derived files remain gitignored. The target hash is over the complete 0x7E000-byte PS-X EXE, including its 0x800-byte header.

### CTR-02 — Provision the selected disc and executable reproducibly
- status: re-verified
- deps: CTR-01
- evidence: C002/I002. `tools/provision.py` resolved the real CTR USA CHD, extracted `SYSTEM.CNF` and `SCUS_944.26` transactionally, and verified the recorded 516096-byte size, SHA-256, PS-X EXE entry/load/text/stack fields. The hermetic selftest accepted a matching fixture and rejected a one-byte mutation and wrong `SYSTEM.CNF`; a real unrelated Spider-Man 2 CHD was also refused before replacing the valid output.
- where: `tools/provision.py`; gitignored `scratch/raw/ctr/{SYSTEM.CNF,SCUS_944.26}`; CMake `provision_selftest` target
- gap: None for reproducible executable provisioning. CTR-03 is now RE-ready; no boot or generated-substrate claim follows from provisioning alone.
- notes: Resolution order is CLI > PSXPORT_CTR_DISC > PSXPORT_DISC > .env game/generic key > deterministic sorted root *.chd drop-in. An invalid higher-priority input refuses instead of silently falling through.

### CTR-03 — Bring up a deterministic psxport/oracle boot harness
- status: re-verified
- deps: CTR-02
- evidence: C003/I003. The asset-gated `oracle_boot_check` re-provisioned the measured executable, ran the independent oracle's 22-check positive/negative/stepping/mirroring fixture, then executed the real CTR crt0 in the vendored Beetle/Mednafen CPU. The execution left mapped text at the InitHeap boundary after 92,378 steps and agreed with the independent symbolic decoder on 7 of 7 comparable fields.
- where: CMake `oracle_boot_check`; framework `oracle_trace` and `crossvalidate_crt0.py`; gitignored boundary trace
- gap: None for the independent first-call oracle. CTR-04 records the later boundary evidence; no later BIOS or hardware behavior follows from this step alone.
- notes: The asset-gated target is deliberately separate from normal verification. Its oracle fixture demonstrates both a clean executed program and a named hardware-stop answer before the real executable is accepted as evidence.

### CTR-04 — Preserve the current execution frontier
- status: re-partial
- deps: CTR-03
- evidence: C004/I004, C005/I005, C006/I006, C007/I007, C009/I008, C010/I009, C011/I010, C012/I011, C013, and C017/I012. `tools/emit_substrate.py` re-verified the complete USA executable identity before invoking the shipping emitter. The executable-header entry, direct-call discovery, and five measured main re-entry roots now emit 1,457 functions in eight shards (recompiler version 2026-08-26.14). The chained gates preserve the independent pre-BIOS 34/34 proof, explicitly model A(39h) InitHeap and A(2Bh) memset, validate every replayed code/data range, and agree with generated execution at each reproducible resident boundary. On exact clean framework `99a42aa3`, the dispatcher window proved 34/34 at indirect target `0x800772E0`; its forced register mismatch produced the sole 33/34 mismatch. The exact operator-run product exited zero after loading `SCUS_944.26`, entering `0x8007793C`, servicing InitHeap, and reaching its supported stop at `0x800772E0`; it explicitly reported that gameplay is unavailable.
- where: `game/recomp_seeds.json`, `game/app/main.cpp`, `game/core/{ctr_runtime,recomp_register,frame_driver,runtime_composition,crt0_port_trace}.{h,cpp}`, `game/core/native_ownership.h`, `tools/emit_substrate.py`, `tools/compare_crt0_trace.py`, `tools/compare_crt0_trace_selftest.py`, `tools/resident_replay.py`, CMake `ctr04_*_check` chain ending in `ctr04_startup_zero_fill_next_call_check`; gitignored `generated/`, executable, replay, and trace outputs under `scratch/`
- gap: The reproducible independent window stops before the first instruction of `0x800772E0`. Claims C014 and C015 remain falsified because their device/zero-fill measurements depended on an unlanded oracle capture slice; `0x800777E8` and `0x80080260` are not verified boundaries. Static RE identifies `0x80080260` as A0:13 `setjmp`, and the later path reaches Timer1 MODE at `0x1F801114`, which requires measured timer semantics. Any new comparison is made against the Lightrec product or separate oracle, never by extending the static route.
- notes: The generated tracer takes interception targets from canonical oracle output and independently requires them in the generated registry. The resident replay is not general continuation: each captured boundary is valid only because every traversed code range, executable-backed data input, and stack-write span is mechanically checked and excluded from trampoline placement. External leaves with RAM effects are boundaries, not register-only returns; the A(2Bh) memset is the first modeled RAM-mutating leaf, and its model is proven by destination poisoning — a missing or wrong write cannot pass silently. Device state is a separate evidence surface; CPU equality cannot hide a DPCR mismatch. The emitter reported 28 unresolved `lw $ra` bases, so the emitted entries are substrate inventory, not a claim that every unexecuted return edge is resolved.
  This complete static-path narrative is retained only as recorded evidence. Do not regenerate,
  build, run, or extend it; new execution evidence comes from Lightrec, the independent emulator or
  hardware, or binary analysis.
  Product ownership is wider than execution proof: static RE grounds resident main `0x8003C58C`,
  state-3 frame owner `0x80035E70`, its post-timing suffix `0x80037880`, and return/resume
  `0x8003CEB4`. `FrameLoopShell` delegates one finite step to CTR's driver; the driver preserves raw
  generated supers and extracts three direct VSync calls plus the frame owner's conditional debug
  VSync. The emitted denominator is 33 direct sites in 20 owner functions. The title now owns the
  statically reachable state-0 VSync(2) route at `0x80031FDC` as a finite two-field wait, with emitted
  post-call/caller continuations at `0x80032074`, `0x8003C8D4`, and `0x8003C984`. A fresh-Clang
  product run instead stopped earlier at stock libcd CdSync `0x8007B6F0`, whose first VSync(-1)
  return was `0x8007B72C`. Binary-backed native CdSync/CdControl bindings now replace the four
  VSync(-1) sites in those synchronous library leaves. PID 2554905 installed all five platform HLE
  entries and advanced beyond that caller, then the shared loop shell refused because main had not
  run its mandatory `prepareProduct` preflight. Main and the composition test now use that owner and
  pass the focused Clang gate. PID 2571678 then live-proved both owners while the other 24 retail
  routes remained fatal, and advanced to shared BIOS A0:1A memcmp at caller
  `0x8001C5D4`; psxport's generic libc implementation is the next dependency. Issue 0015 owns this
  advancing boundary. That shared implementation now passes its focused Clang test and CTR is
  relinked against it. PID 2590436 advanced to VSync(-1) return `0x8007705C` inside stock libcd
  CdRead `0x80076F10`; Ghidra identifies all three polling sites in that async controller/IRQ body.
  Immediate CdReadSync `0x800770AC` owns two additional polls. The existing shared synchronous
  stock-read owners now expose narrow typed direct-runtime interfaces. CTR binds both measured
  leaves to those newly public shared owners and its DiscState to
  `PSXPORT_CTR_DISC`; shared and focused CTR Clang tests pass. Real-disc PID 2665477 live-passed
  both stock-read owners, opened the CHD, then reached the next distinct VSync(-1), return
  `0x800750B8`, in libgpu timeout arm `0x800750A8`. Ghidra proves that function and companion
  timeout check `0x800750DC` read VSync only as the clock for deadline `0x8008AEBC` and poll count
  `0x8008AEC0`. CTR now binds both to the host-owned `Timing::vblank` counter and aborts rather than
  concealing a violated synchronous-GPU drain contract; the focused Clang test passes and live proof
  is queued. PRESENT and guest-VRAM captures at frames 0-4 produced no files because execution never
  reached a present. PID 2714292 then passed the GPU-timeout pair and remained live for the full
  20-second bound without completing a fence. PID 2718506's three-second watchdog resolved the
  exact stack as `0x800293B8 <- 0x8002DD24 <- 0x8003C8D4`, reached after the first finite resource
  wait resumed. Ghidra proves `0x8002DD24` is two do/while polls around asynchronous resource stages;
  keeping them inside one guest dispatch starves the native field/CD service. The driver now
  preserves the generated setup and poll functions, yields after each false result, and resumes the
  exact caller suffix through emitted entry `0x8003C8FC`. The focused Clang transition test covers
  both yields and guest-frame restoration. Real-disc execution crossed that boundary and isolated
  DMA4 owed during custom-exception dispatch at PC `0x8007B104`, RA `0x80077494`. Ghidra and live
  RAM agree on callback table `0x8008CB08`, slot `0x8008CB18`, wrapper `0x8007AB34`, and app callback
  `0x8001C984`. B0:17 unwinds through `0x80077348`, so CTR instead owns safe post-unwind loop seam
  `0x8003C94C`, preserving DICR, HLE `in_irq`, R3000, and finite chained-transfer ordering. A
  concurrent-game diagnostic crossed that seam and next found resource-wait continuation
  `0x800336F8`; it is not serialized product proof. Static retail code proves its fifth argument is
  `-1`, so the same two-field owner admits that emitted reentry. Serialized PID 3197275 crossed the
  continuation without reaching guest VSync, then the title guard reported that its generated
  suffix returned before a frame boundary. The retail generated call graph grounds that return:
  `0x800336F8` is an interior suffix of `0x80033610`, whose sole direct caller resumes at
  `0x8003CC98`. That exact reentry is now emitted and required before dispatch. The focused
  transition contract proves both native wait fields, stack restoration, the suffix return, and
  continuation to a frame fence; the full Clang gate passes. PID 3229162 then live-crossed that
  reentry with no guest VSync and reached
  timing leaf `0x8004B3A4` from return `0x8004B438`. Static retail code establishes it as the leaf's
  only other direct caller: wrapper `0x8004B41C` uses the result as an elapsed-time query and contains
  no VSync, while return `0x8003785C` alone leads to the conditional debug VSync and frame suffix.
  The title route now preserves and returns from the generated timing super for `0x8004B438`, retains
  frame completion only for `0x8003785C`, and aborts every other return. Focused composition and full
  Clang gates pass. A clean-fb08d30f rerun then exposed one title-adapter omission before the next
  field: the scoped VSyncCallback owner at `0x80077254` requested its raw generated super, but the
  preserved-super map lacked that exact address. The map now dispatches `gen_func_80077254`, and the
  focused runtime contract requires the callback owner to request it. Serialized PID 3531982 crossed
  that repaired boundary, registered retail callback `0x80034AA4`, initialized the actual Vulkan GTE
  presenter, and submitted the first 960x720 headless product image without reaching guest VSync.
  It was stopped at that answer; pixels and sustained cadence remain unmeasured.
  C019 then owns the first pixel inspection and its cause. Present-stage and guest-VRAM captures at
  fields 1-4 were entirely black; watchpoints on `gp`+0x188/0x18C proved resident main reached state 3
  once and `0x80033610` returned stage 2 for 161,226 consecutive fields, because its busy flag at
  `gp`+0x138 never cleared. Ghidra grounds the chain: `FUN_800334F4` sets that flag and queues a load,
  the pump `FUN_80032DC0` dispatches it through `FUN_800321B4`, which registers a libcd completion
  callback through `CdReadCallback` `0x800771B0` (slot `0x8008AD10`) and does not poll. A watchpoint on
  that slot showed `0x80032110` registered once and never cleared, while the earlier hand-published
  `0x8003254C` path cleared four times. CTR now owns the CdRead leaf `0x80076F10`: the shared native
  transfer, then a dispatch of the exact registered callback with `CdlComplete` and full register
  restoration; the transcribed `0x80032594` owner is removed rather than annotated. A serialized
  real-disc rerun advanced the loader 2, 3, 4, 5 across fields 1-4 and then failed fast on a
  recomp-MISS at `0x800B0B38` from caller `0x800368BC` — disc-loaded overlay code the emitter reported
  as `0 overlay module(s)`.
  C020/I016 then ground the overlay layer. `BIGFILE.BIG` is identity-verified and its index parses as
  608 monotonic (sector offset, byte size) entries from word 2; `PSXPORT_DEBUG=cd` named archive
  entries 225, 226 and 233 loading to `0x8009F6FC`, `0x800A0CB8` and `0x800AB9F0`, with `0x800B0B38`
  inside entry 233. `tools/extract_overlays.py` slices exactly those entries at their index byte size
  (the sector-padded slice made entry 226 overlap entry 233's base, which the emitter reported), and
  `emit_substrate.py` refuses to emit when a declared overlay has no image. The three modules
  recompiled as 1, 270 and 152 functions. Five loader completion callbacks reached only through the
  queue's function-pointer slot are now `main` seeds, each proven a clean entry by the `jr ra` two
  words before it. The next real-disc run crossed all of that, performed four further module loads,
  and reached state-1 `0x8003B934` before faulting in scratchpad helper `0x8006D79C`. Issue 0021 shows
  that was a delivery-TIME defect in the callback of issue 0019, not a missing overlay: retail cannot
  run the completion inside CdRead, because `FUN_80031E00` stores the allocated buffer into the queue
  entry only after `FUN_800321B4` returns and the completion chain `FUN_80031D30` reads that field to
  run the module's relocation pass (`FUN_800326B4` adds the load base to each offset in a patch
  table).   The owner now records an owed completion and delivers it at the title's per-field seam and
  before any subsequent read. The product then crosses the null dereference, performs its later
  module loads, presents at least 16 fields, and reaches the hand-written GTE library, failing fast on
  an unresolved computed-jump continuation — `0x8006ACE0` reached from `jr t2` at `0x8006C948`, a
  helper with no `jal` site anywhere in the executable (issue 0022).
  Issue 0022 is now RESOLVED by measurement: the library uses t2 as an alternate link register
  (`jalr t2, v1` with the return address in t2; $ra holds a runtime-built 244-byte-stride parameter
  block whose heap blocks hold nine distinct library interior entries). The framework emitter
  (RECOMP_VERSION 2026-08-28.1, dirty awaiting operator landing) now derives every non-ra jalr link
  (35 sites → 35 continuations on CTR MAIN) as a dispatchable re-entry and forgets
  coroutine-resume proofs at re-entry boundaries (MAIN false coroutines 8 → 4 of 1255); red/green
  in tools/recomp/test_emit.py (61/61), decoder 9/9, framework ctest 119/119, CTR asset-free
  ctest 14/14. The two block-slot entries no scan can link (0x8006A8E0, 0x8006BF30 — the
  constructor-pointer scan cannot tie a store site to a dispatch site across fragments, Vagrant
  issue #23's open class) are `main_reentry` seeds with provenance. The serialized real-disc run
  then executes the whole chain end-to-end, repeatedly, and fails fast later INSIDE the chain on a
  garbage a2=0x0CC22321 read at a2+0x74 — issue 0023 owns that boundary; scratch/logs/altlink-live4.log.
  Platform composition binds measured VSync `0x80075350` to the fatal trap. The asset-free
  transition test covers startup, repeated frame resume, and teardown, but independent generated
  execution remains proven only through `0x800772E0`. A fresh Clang 22.1.8 tree against clean
  psxport `319d30b62ba6bc417bb8edb518b5eaae97750825` builds every target and passes the complete
  asset-free 8/8 CTest plus normal `verify` gate; pin, checkout, and the selected build provenance
  record agree exactly. The provenance gate now receives the active CMake build record and has
  positive plus mismatched-record tests; issue 0018 records the former fixed-directory defect.
  Executable process contracts additionally prove both `--help` and `-h` exit zero before asset
  discovery. This integration evidence does not advance the live frontier.

## Native ownership and enhancements

### CTR-05 — Identify camera state and graphics submitters
- status: re-partial
- deps: CTR-04
- evidence: C016/I015. On identity-verified SCUS_944.26, `tools/measure_render_frontier.py` proves the exact libgte leaves `SetGeomScreen [0x8007781C,0x80077828)` and `SetGeomOffset [0x8007782C,0x80077844)`, their direct callers, and the complete raw CR24/CR25/CR26 text-word census (17/17/16). Boot sets OFX=256, OFY=120, H=320. Function `[0x80042910,0x80042974)` derives OFX/OFY/H from view offsets `+0x20/+0x22/+0x18` and is called at `0x80024CCC`, `0x8003BD2C`, and `0x8003F5C0`. Ghidra independently decompiled `[0x80024C4C,0x80025138)` as a `lensflare` primitive producer: it calls `0x80042910`, runs MVMVA plus three RTPT operations, copies SXY results into four `0x0C`-tagged packets, and inserts the packet chain into the ordering table. Registrar `0x80025138` passes `0x80024C4C` as a callback to `0x8004205C`; a complete raw JAL scan finds no direct caller.
- where: `tools/measure_render_frontier.py`; `game/core/platform_hle_plan.{h,cpp}`; Ghidra outputs under gitignored `scratch/decomp/render/`; CMake `ctr05_render_frontier_{selftest,check}`
- gap: This is a static producer boundary, not a visible-frame or camera-ownership proof. The typed leaf HLE observes only calls through the two libgte setters; 16/16/15 other raw CR24/CR25/CR26 words bypass those leaves. After CTR-JIT-02 reproduces the current live frontier, a serialized Lightrec capture must identify the active `0x8004205C` registration and indirect producer callbacks, then follow the view object to simulation-owned camera/transforms. OT, GP0, SXY, and quantized GTE output remain diagnostic evidence, never native producer input.
- notes: The CTR runtime supplies only the two measured addresses and exact half-open library window to framework-owned typed handlers. The test records the measured boot projection through those shipping handlers; it does not enable widescreen or claim native rendering.
  C018 records the title's scoped `0x80042910` A/B override: `ProjectionOwner` captures its pre-GTE view
  width/height/centre/H, calls the raw generated super, and refuses if the published libgte values
  disagree. This grounds the first projection-publication owner but still does not identify a native
  camera transform or widen projection/culling/layout.

### CTR-06 — Native widescreen
- status: todo
- deps: CTR-05, CTR-08
- evidence: Not started.
- where: future native camera and render producers
- gap: The current projection HLE only records guest OFX/OFY/H and has 16/16/15 other raw CR24/CR25/CR26 writes outside its two leaves. Enable widescreen only after CTR-08 owns the frame's native camera/projection and display-list producers, then change the producer's aspect/viewport inputs and prove culling, HUD, and presentation bounds. Do not stretch the guest image or patch OFX/H constants.

### CTR-07 — Transform interpolation
- status: todo
- deps: CTR-05, CTR-08
- evidence: Not started.
- where: future PC-owned transform producers
- gap: The port has a PC-owned finite transition driver, but no native simulation owner and no previous/current camera or object-transform history. Following Dusklight's ownership pattern, simulation must first produce current native transforms; a separate temporal presentation decorator may then interpolate previous/current camera and object state. Do not interpolate or invert quantised GTE SXY/OT output.

### CTR-08 — Reach the first visible frame and own a native primitive renderer
- status: re-partial
- deps: CTR-JIT-02, CTR-05
- evidence: Identity-gated static RE identifies resident main `0x8003C58C`, loop top `0x8003C5D0`, the state-3 call to frame owner `0x80035E70`, its timing call return `0x8003785C`, post-VSync suffix `0x80037880`, and main resume `0x8003CEB4`. The title driver preserves generated supers around these exact boundaries and a production-composition test advances startup plus repeated and teardown transitions without allowing VSync to return. `PresentationOwner` rotates exactly one fence at every measured return, committing captured retail work and marking empty fields unpresented. C016 separately identifies projection producer `0x80042910` and a binary-grounded primitive-producer family; the projection owner preserves the generated super and checks the retail publication against pre-GTE view input. Capability exposure selects GTE and refuses Native/temporal products which do not exist. Serialized PID 3531982 crossed the later frame/callback chain, initialized the Vulkan GTE presenter, and submitted a 960x720 product image with the fatal VSync trap intact. The run stopped before screenshot/pixel inspection, so this is presentation-boundary evidence rather than a visible-content or native-renderer claim.
- where: future frame driver, producer-owned camera/transforms, render queue, and native renderer under cohesive `game/` modules
- gap: C019 measured the first captured compatibility images as empty because the state-3 loader's retail CD completion never arrived; that timing defect and the later overlay-discovery gap were resolved before issue 0023 became the live boundary. CTR-JIT-02 must first reproduce that current boundary through Lightrec. A later live capture identifies active projection/producer callbacks and sustained cadence. Native render ownership then replaces one dynamically observed producer with a native override and Lightrec original-call comparison; the host renderer consumes producer-owned primitives and depth/order, never framebuffer pixels. Native and temporal capabilities remain unavailable until their owners exist.
- notes: This step is deliberately the common dependency for widescreen and interpolation. Visible guest output alone would not satisfy native render ownership.

## Runtime execution migration

### CTR-JIT-01 — replace generated dispatch and `FrameCompleted` unwinding
- status: todo
- deps: CTR-02, CTR-03, CTR-04
- evidence: The portfolio plan selects psxport's pinned Lightrec integration. `game/core/frame_driver.cpp` currently throws and catches local `FrameCompleted` at wait, service, and frame-completion boundaries; that host-stack unwind cannot cross JIT frames.
- where: psxport's target per-`Core` executor; `game/core/{ctr_runtime,runtime_composition,frame_driver,native_ownership}.*`; existing native owner modules
- gap: Map authenticated resident/overlay images, register native overrides by image generation plus address, replace generated dispatch and `super` calls with executor dispatch/original calls, and replace every `FrameCompleted` throw with a typed executor-exit request. Lightrec must return normally at a safe boundary with synchronized state; the frame driver validates the reason, restores diagnostic depth, finishes exactly one field fence, and rejects stale/unknown exits.
- notes: Product link/selector inspection must prove nonzero Lightrec execution and the absence of both interpreter and generated guest code. `longjmp` is not an acceptable translation of the exception.

### CTR-JIT-02 — reproduce the current live frontier through Lightrec
- status: todo
- deps: CTR-JIT-01
- evidence: Issue 0023 records the current boundary: the alternate-link GTE chain executes, its input pair at `0x8010B2D4` is already corrupt, and `lw v1,0x74(a2)` at `0x8006AB00` faults without invalidating earlier verified paths.
- where: native/Lightrec CTR product with current CD, DMA, frame, projection, and presentation owners
- gap: Reach the same boundary with nonzero Lightrec blocks, prove one native override plus scoped original call, and demonstrate overlay replacement invalidates the affected translated blocks with positive and controlled-negative cases.
- notes: This is the first wiring discriminator. It does not authorize static-corpus deletion and must not patch the GTE consumer or substitute a pointer.

### CTR-JIT-03 — prove representative interactive gameplay
- status: todo
- deps: CTR-JIT-02
- evidence:
- where: CTR gameplay product plus separately built independent-oracle harness
- gap: Continue beyond the preserved frontier to a representative playable race with correct input, audio, presentation, timing, interrupts, memory and relevant device state; compare deterministic boundaries and qualify released host architectures.
- notes: Only this gate authorizes deletion of the static generator, corpus, dispatcher/registry, seeds, generated-body adapters, generated-symbol tests, and stale methodology. No compatibility mode remains.
