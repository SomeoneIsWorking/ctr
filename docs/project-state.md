# Project state

Epic intent lives in `docs/project-goals.md`, migration order in `docs/migration.md`, ownership in
`docs/codemap.md`, per-step reverse-engineering status in `docs/re-frontier.md`, and atomic work in
`docs/issues/`. The comparison baseline is the North American retail game under an accurate vanilla
PlayStation emulator; every row below is a user-visible delta from it.

| ID | Capability | State | Evidence or gap |
|---|---|---|---|
| S001 | USA disc and `SCUS_944.26` reproducibly identified and provisioned | verified | `SYSTEM.CNF` selects `SCUS_944.26`, LBA 24, 516,096 bytes, SHA-256 `7b4aac0b…efb838`, load `0x80010000`, text `[0x80010000,0x8008D800)`; `tools/provision.py` extracts transactionally and refuses a mutated image |
| S002 | Independent CPU execution establishes deterministic retail boot boundaries | verified | Beetle/Mednafen CPU reached InitHeap after 92,378 instructions, agreeing 7/7 with an independent crt0 decoder |
| S003 | Preserved evidence reaches the current resident and live execution frontiers | partial | comparison reached pre-instruction `0x800772E0` 34/34 fields (forced 33/34 negative), tracked as steps CTR-01..CTR-04 and CTR-JIT-* in `docs/re-frontier.md`; issue 0023 is the live boundary and must be reproduced through Lightrec |
| S004 | Projection and primitive-producer source boundaries grounded in the executable | partial | libgte leaves `SetGeomScreen [0x8007781C,0x80077828)`, `SetGeomOffset [0x8007782C,0x80077844)`, publication `[0x80042910,0x80042974)`, lens-flare producer `[0x80024C4C,0x80025138)` (frontier step CTR-05); the ten submitters' packet-build step and the bounding 2D layers are still retail code |
| S005 | Frames produced by a game-state native renderer | missing | no native camera/object producer set, render queue, ordering/depth owner, or renderer; the run faults at `0x8006C0FC` in field 29,033 (issue 0024), so the only 3D content reached is the attract sequence |
| S006 | Native camera and projection support true widescreen | partial | canvas widens 512 -> 684 and guest 3D is re-projected (911 of 110,553 transforms take the plan's H); the drawn band is still 719 of 960 columns because 14,822 of 67,282 submitted prims are untouched 2D (issue 0032), and 0 `BIGFILE.BIG` overlays are covered |
| S007 | Native camera and object transforms interpolated for presentation | missing | in scope: 2 fields per game frame = 30 fps, paced by a two-field countdown at `[gp+0x348]` drained by the vertical-blank callback, not by the single boot-time `VSync(2)`; no native simulation owner or previous/current transform pair exists yet |
| S008 | Default product reaches sustained playable gameplay with input and audio | missing | boot, first submitted image, and the attract/FMV sequence are not a playable race |
| S009 | Native/Lightrec product reaches the preserved frontier without an interpreter mode or generated code | verified | MEASURED 2026-10-08: with BF0229/BF0230/BF0231 measured and the issue 0033 fixes, boot loads every overlay, presents frames (`frame` > 0) and reaches the intro, demo races and main menu. |
| S010 | Frame/service suspension uses explicit typed executor exits | partial | `frame_driver.cpp` requests `FrameBoundary` and returns normally; asset-free focused coverage preserves reason, PC, cycles, detail, and refuses a zero-cycle host loop; real-game nested and repeated exits beyond the `0x8006AA80` fault are unproven |
| S011 | Load operations complete without loading-only waits or presentation | missing | no load issuer, wait, or presentation has been censused for CTR; logos still need a cancellation route |
| S012 | CTR picture is the guest's GP0 work replayed on the Record path, oracle-exact at 4:3 | partial | MEASURED 2026-10-08, 1x 4:3 through intro, demo races and menu: 21,041 present lines at fps60 off with 170 mismatched, 19,490 at fps60 on with 170 mismatched (same seqs). Mismatches are +-1 per channel in 12-130 pixel clusters from seq 11086, 3D frames only; cause not pinned (issue 0033). Shots in `scratch/record/`. |

Current focus: S009 — MEASURED 2026-10-04: `0x200` is I_STAT bit 9, the SPU line (`IRQ_BIT_SPU`), not
the DMA line (bit 3), and "no SysEnq element claimed it" was a diagnostic, not the stall: the guest's
one element declines it and its custom exception exit services it (the guest acks I_STAT 0x5FF at
`0x80077358`). The stall was state zero's CD-XA wait at `0x8003C94C` on the guest's playback-state word
`[0x8008D708]`, which only the guest's own libcd callbacks move: the CD data-ready reached libcd's SYNC
slot `0x8008C41C` (`0x8001C7A4`, code 2) instead of its DATA-READY slot `0x8008C420` (`0x8001C7FC`,
code 1), and the controller skipped the XA audio sectors. The slot declaration is fixed. The wait on `[0x8008D708]` is fixed (issue 0033: a filter-rejected XA sector is dropped as on Beetle, the
CD callback runs in the BIOS handler context, the SPU DMA callback is registered). The overlays are measured and CTR now presents.
Current focus: S012's 170 recordcheck mismatch lines (issue 0033).

Hosted CI is asset-free: the Linux x86_64 job builds the native/Lightrec product, runs its focused
tests, and inspects the linked executable for forbidden static or standalone-interpreter ownership.
It proves compilation and composition only. Real gameplay and oracle comparison require user media.
Windows x86_64, macOS arm64, and Android arm64-v8a have no truthful title job: the shared
PSXPort/Lightrec Windows and Apple Silicon product builds and the shared Android packaging plus
CTR touch/setup ownership are not complete.

Known gaps tracked as issues: 0023 (corrupt GTE descriptor pair), 0024 (Lightrec + typed exits),
0027 (projection census reported from one of thirteen refusal sites), 0028 (frame-timing bridge
dereferences an unchecked game-state pointer), 0032 (2D primitives bound the widened margin).
Issue 0029 (overrides selecting their instance through a process global) is resolved title-side: each
override now resolves its owner from the `Core` it was handed.

Not covered: `game/video/projection_owner.cpp` refuses a publication disagreement with
`std::abort()`, and no portable death-test facility exists, so that branch has no automated
coverage. The offsets the owner reads are pinned against its own source, not against the retail
image.
Frame boot and control channel: `verified` against psxport `897726f2` — `game/entry/main.cpp` is
now six steps on `psx::Machine` (`bindDevices`, `prepare`, `attachControlChannel`, `run`), override
install and removal both go through `psx::cpu::{tryInstallNativeOverride,removeNativeOverride}`, and
the title owns no `(image identity, address)` keying. Headless boot log is unchanged except for the
framework's own control-channel and loop-entry lines and the reordering of the platform-HLE
registration log, which `Machine::bindDevices` performs before the entry line.
