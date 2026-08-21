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
- evidence: C004/I004, C005/I005, and C006/I006. `tools/emit_substrate.py` re-verified the complete USA executable identity before invoking the shipping emitter. With an empty explicit seed manifest, the executable-header entry plus direct-call discovery emitted 1,236 functions in eight shards (recompiler version 2026-08-12.1). `ctr04_check` preserves the independent pre-BIOS proof at 34/34 fields. `ctr04_post_init_heap_check` ran the independent CPU twice with identical state/step evidence, verified the observed A(39h) vector and applied the framework's explicit `v0=0` leaf contract, then compared initial-call, modeled-return, and first-subsequent-call state against generated execution: 108/108 fields agreed at `0x8003C58C`; forced `post.gp=0` produced 107/108. Independent Ghidra disassembly of original `0x8003C58C..0x8003C5AC` proved a store-only prefix before its first call. `ctr04_resident_next_call_check` validates those exact bytes, repeats both original-state and bounded-replay oracle runs, and uses canonical `--capture-call 1` to discover `0x800779E4`; generated execution agreed 34/34 there and forced `resident.gp=0` produced 33/34.
- where: `game/recomp_seeds.json`, `game/core/crt0_port_trace.cpp`, `tools/emit_substrate.py`, `tools/compare_crt0_trace.py`, `tools/resident_replay.py`, CMake `ctr04_check`/`ctr04_post_init_heap_check`/`ctr04_resident_next_call_check`; gitignored `generated/`, `scratch/raw/ctr/ctr04-resident-replay.exe`, and boundary traces
- gap: The verified window stops on resident call `0x800779E4`, with `$ra=0x8003C5B0`. Prove that callee's next call or hardware boundary without assuming replay RAM equals post-crt0 RAM; do not treat the explicit external-leaf model as a general BIOS and never guess an overlay base.
- notes: The generated tracer takes interception targets from canonical oracle output and independently requires them in the generated registry. The resident replay is not general continuation: it is valid only because its exact checked prefix has no RAM reads before the captured call, and it excludes that prefix's stack-write range when placing its trampoline. The emitter reported 28 unresolved `lw $ra` bases, so the 1,236 emitted entries are substrate inventory, not a claim that every unexecuted return edge is resolved.

## Native ownership and enhancements

### CTR-05 — Identify camera state and graphics submitters
- status: todo
- deps: CTR-04
- evidence: Not started.
- where: future Ghidra project and readable native ownership under `game/`
- gap: Decompile the game code that submits camera/transforms/geometry before creating any native producer. OT, GP0, and GTE output are diagnostic evidence, never producer input.

### CTR-06 — Native widescreen
- status: todo
- deps: CTR-05
- evidence: Not started.
- where: future native camera and render producers
- gap: Enable only after the PC owns the relevant camera/projection and display-list producers.

### CTR-07 — Transform interpolation
- status: todo
- deps: CTR-05
- evidence: Not started.
- where: future PC-owned transform producers
- gap: Interpolate only values computed by native producers; do not interpolate or invert quantised GTE results.
