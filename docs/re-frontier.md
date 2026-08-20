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
- status: todo
- deps: CTR-01
- evidence: Not started.
- where: future project-local provisioning tool; gitignored `.env` or root drop-in input
- gap: Implement CLI argument > `PSXPORT_CTR_DISC` > `.env` > root-drop-in resolution, extract `SCUS_944.26` to `scratch/`, and verify its SHA-256 before any recompilation.

### CTR-03 — Bring up a deterministic psxport/oracle boot harness
- status: todo
- deps: CTR-02
- evidence: Not started; `ctr_scaffold` only links `psxport_smoke` and runs no CTR code.
- where: future `game/core/`, generated substrate, and project-owned gate
- gap: Build the first game seam and oracle driver, then prove the harness reports both an intentional agreement and an intentional disagreement on permanent fixtures.

### CTR-04 — Recompile through the first real divergence
- status: todo
- deps: CTR-03
- evidence: Not started.
- where: future `generated/`, `game/recomp_seeds.json`, and divergence logs
- gap: Recompile from the measured entry and advance only as far as executable evidence supports; never borrow another game's seeds or guess an overlay base.

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
