# CTR RE frontier

Statuses: `re-verified` means binary/disc ground truth plus executable verification; `re-partial`
names an honest remaining gap; `todo` is not started. No hacks are tracked.

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
- gap: None for the independent first-call oracle. CTR-04 now owns the generated side; no later BIOS or hardware behavior follows from this step.
- notes: The asset-gated target is deliberately separate from normal verification. Its oracle fixture demonstrates both a clean executed program and a named hardware-stop answer before the real executable is accepted as evidence.

### CTR-04 — Recompile through the first real divergence
- status: re-partial
- deps: CTR-03
- evidence: C004/I004, C005/I005, C006/I006, C007/I007, C009/I008, C010/I009, C011/I010, C012/I011, C013, and C017/I012. `tools/emit_substrate.py` re-verified the complete USA executable identity before invoking the shipping emitter. With an empty explicit seed manifest, the executable-header entry plus direct-call discovery emitted 1,448 functions in eight shards (recompiler version 2026-08-26.14). The chained gates preserve the independent pre-BIOS 34/34 proof, explicitly model A(39h) InitHeap and A(2Bh) memset, validate every replayed code/data range, and agree with generated execution at each reproducible resident boundary. On exact clean framework `99a42aa3`, the dispatcher window proved 34/34 at indirect target `0x800772E0`; its forced register mismatch produced the sole 33/34 mismatch. The exact operator-run product exited zero after loading `SCUS_944.26`, entering `0x8007793C`, servicing InitHeap, and reaching its supported stop at `0x800772E0`; it explicitly reported that gameplay is unavailable.
- where: `game/recomp_seeds.json`, `game/app/main.cpp`, `game/core/{ctr_runtime,recomp_register,bootstrap_frontier,crt0_port_trace}.{h,cpp}`, `tools/emit_substrate.py`, `tools/compare_crt0_trace.py`, `tools/compare_crt0_trace_selftest.py`, `tools/resident_replay.py`, CMake `ctr04_*_check` chain ending in `ctr04_startup_zero_fill_next_call_check`; gitignored `generated/`, executable, replay, and trace outputs under `scratch/`
- gap: The reproducible execution window stops before the first instruction of `0x800772E0`. Candidate initializer-device and poisoned zero-fill gates remain implemented, but claims C014 and C015 are falsified because their earlier measurements depended on an unlanded oracle `--capture-devices` slice. Exact `99a42aa3` refuses that option, so issue 0014 must land the shared capture interface and rerun 37/37 plus 41/41 negative-controlled comparisons before `0x800777E8` or `0x80080260` can re-enter the verified frontier. Static RE identifies `0x80080260` as A0:13 `setjmp`; the later path reaches Timer1 MODE at `0x1F801114`, which also requires proper timer semantics rather than a discarded write.
- notes: The generated tracer takes interception targets from canonical oracle output and independently requires them in the generated registry. The resident replay is not general continuation: each captured boundary is valid only because every traversed code range, executable-backed data input, and stack-write span is mechanically checked and excluded from trampoline placement. External leaves with RAM effects are boundaries, not register-only returns; the A(2Bh) memset is the first modeled RAM-mutating leaf, and its model is proven by destination poisoning — a missing or wrong write cannot pass silently. Device state is a separate evidence surface; CPU equality cannot hide a DPCR mismatch. The emitter reported 28 unresolved `lw $ra` bases, so the 1,448 emitted entries are substrate inventory, not a claim that every unexecuted return edge is resolved.

## Native ownership and enhancements

### CTR-05 — Identify camera state and graphics submitters
- status: re-partial
- deps: CTR-04
- evidence: C016/I015. On identity-verified SCUS_944.26, `tools/measure_render_frontier.py` proves the exact libgte leaves `SetGeomScreen [0x8007781C,0x80077828)` and `SetGeomOffset [0x8007782C,0x80077844)`, their direct callers, and the complete raw CR24/CR25/CR26 text-word census (17/17/16). Boot sets OFX=256, OFY=120, H=320. Function `[0x80042910,0x80042974)` derives OFX/OFY/H from view offsets `+0x20/+0x22/+0x18` and is called at `0x80024CCC`, `0x8003BD2C`, and `0x8003F5C0`. Ghidra independently decompiled `[0x80024C4C,0x80025138)` as a `lensflare` primitive producer: it calls `0x80042910`, runs MVMVA plus three RTPT operations, copies SXY results into four `0x0C`-tagged packets, and inserts the packet chain into the ordering table. Registrar `0x80025138` passes `0x80024C4C` as a callback to `0x8004205C`; a complete raw JAL scan finds no direct caller.
- where: `tools/measure_render_frontier.py`; `game/core/projection_hle_plan.{h,cpp}`; Ghidra outputs under gitignored `scratch/decomp/render/`; CMake `ctr05_render_frontier_{selftest,check}`
- gap: This is a static producer boundary, not a visible-frame or camera-ownership proof. The typed leaf HLE observes only calls through the two libgte setters; 16/16/15 other raw CR24/CR25/CR26 words bypass those leaves. After CTR-04 first restores reproducible device/zero-fill evidence, crosses the A0:13 `setjmp` thunk, and implements later timer/hardware semantics, a serialized live capture must prove which `0x8004205C` registration and indirect producer callbacks execute in the first real frame, then follow the view object back to its simulation-owned camera/transforms. OT, GP0, SXY, and quantized GTE output remain diagnostic evidence, never native producer input.
- notes: The CTR runtime supplies only the two measured addresses and exact half-open library window to framework-owned typed handlers. The test records the measured boot projection through those shipping handlers; it does not enable widescreen or claim native rendering.

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
- gap: The port has no native simulation frame driver and no previous/current camera or object-transform history. Following Dusklight's ownership pattern, simulation must first produce current native transforms; a separate temporal presentation decorator may then interpolate previous/current camera and object state. Do not interpolate or invert quantised GTE SXY/OT output.

### CTR-08 — Reach the first visible frame and own a native primitive renderer
- status: todo
- deps: CTR-04, CTR-05
- evidence: Not started. C016 identifies a binary-grounded first primitive-producer family but does not prove that it executes in the first visible frame.
- where: future frame driver, producer-owned camera/transforms, render queue, and native renderer under cohesive `game/` modules
- gap: First restore the landed device-capture dependency and reproduce the initializer-device and poisoned zero-fill gates through `0x80080260`; then cross that real A0:13 `setjmp` leaf and implement the measured timer/hardware semantics that follow. In one serialized live run, capture the first display/OT submission, projection setters and inline CR24/25/26 writes, `0x8004205C` registrations, and indirect callbacks. Native ownership begins by replacing one dynamically observed producer with a runtime override that preserves its generated super-call for A/B comparison; the host renderer consumes producer-owned primitives and depth/order, never reverse-engineered framebuffer pixels.
- notes: This step is deliberately the common dependency for widescreen and interpolation. Visible guest output alone would not satisfy native render ownership.
