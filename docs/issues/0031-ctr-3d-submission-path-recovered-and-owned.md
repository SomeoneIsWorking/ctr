# 0031 — CTR's 3D submission path, recovered and owned: the census that said "0 3D prims" was reading the port, not the game

Every number below is a measurement with a denominator. Byte facts come out of the authenticated
`scratch/raw/ctr/SCUS_944.26`; runtime facts come out of headless runs of the shipping product
against the provisioned disc. Where a number could not be obtained, this issue says so instead of
printing a zero.

## 0. The brief's premise was wrong, and it was wrong in the same way twice

The task that opened this work said CTR is "widescreen-invisible: **0 of 73,695 prims is 3D**", and
that "nothing three-dimensional is ever submitted".

**The guest submits Gouraud polygons, and they are almost everything it submits.** Re-reading the
primdump the framework already writes (`scratch/logs/prims_f1.csv`, 73,694 data rows) and counting
the GP0 command byte rather than the framework's own `is3d` column:

| leg | prims scanned | Gouraud-shaded polygons | of those, textured |
|---|---|---|---|
| 4:3 | **73,695** | **73,547 (99.8%)** | **46,213** |
| 16:9 | **67,282** | **67,134 (99.8%)** | **41,317** |

A Gouraud-shaded polygon is the PlayStation's 3D primitive: there is no other 3D primitive form on
the hardware, and `0x34`/`0x36`/`0x30` are the textured-Gouraud quad, textured-Gouraud triangle and
Gouraud quad respectively. The census op histogram is `0x34 × 38,606`, `0x30 × 12,478`, `0x36 × 7,607`,
`0x38 × 522`, `0x2E × 148`, `0x2A × 34`, `0x68 × 14,300`. **"0 of 73,695 prims is 3D" is a fact about
psxport, not about Crash Team Racing.**

## 1. WHY the `is3d` column reads zero, and why nobody caught it

`runtime/psx/gpu_native.cpp:773-789` defines it:

```c
int is3d = 1;
for (int i = 0; i < nv; i++) {
  if (vaddr[i] && core->rsub.projprim.lookupPz(core, vaddr[i], &pz)) { dep[i] = proj_pz_to_ord(pz); }
  else { is3d = 0; break; }
}
```

So `is3d == 1` means **every packet vertex resolved a per-vertex view-Z in the port's `ProjPrim`
cache**. It is a native-depth classification, not a statement about what the guest drew.

`ProjPrim::setPz` is only ever called from three functions in `runtime/psx/gte_beetle.cpp`:
`gte_store_xy` (the `swc2` of a projected screen-XY register), `gte_record_pz` (the `mfc2`+`sw`
pairing) and `gte_copy_pz` (buffer-to-buffer depth carry). **None of the three has a caller anywhere
in psxport.** The header says why in the past tense: "The runtime **translator** pairs `mfc2 rX,
DR12..15` with the `sw rX, off(base)`" — that static translator is deleted, per the workspace
migration. Lightrec's `cop2Operation` calls `gte_op_at` but never `gte_store_xy`.

**Therefore `is3d` is 0 by construction for every title running on Lightrec, and the census had no
denominator for "was the tap ever fed" and never asked.** This is the exact defect the workspace has
already paid for twice: a census reporting 0 for a counter that nothing writes.

**The instrument was then shown to give the other answer, on its own subject.** The new gate
`tools/ctr_binary_probe.py gte-projection` first asserted a single eight-instruction submission
prologue of 10 sites, and **reported 0 of 10 matching it** — a real zero, from a real wrong claim,
produced by the same code path that produces the 10-of-10 below. See §4.

## 2. THE RECOVERED SUBMISSION PATH, in readable terms

`tools/ctr_binary_probe.py gte-projection` derives all of this from the image and is registered in
CTest. Register numbers come from psxport's own GTE, not from a shifted local guess:
`vendor/beetle-psx/mednafen/psx/gte.c:155-157` names `CR[24]=OFX`, `CR[25]=OFY`, `CR[26]=H`.

**The shared tail, 5 instructions, identical at all ten sites, ending AT the `H` write:**

```c
sll  a, a, 15        // OFX/OFY are 16.16 fixed point built from a 16-bit screen offset
sll  b, b, 15
ctc2 a, $24         // -> CR24 = OFX
ctc2 b, $25         // -> CR25 = OFY
ctc2 c, $26         // -> CR26 = H
```

**The ten submitters**, each reached only by `jal` (so each is a valid override key — 0 of the ten is
a `j` target), each carrying the tail verbatim:

| entry | H write | direct `jal` callers | first caller |
|---|---|---|---|
| 0x80069FFC | 0x8006A0E0 | 1 | 0x800364D4 |
| 0x8006AAA8 | 0x8006AB38 | 1 | 0x80036BE0 |
| 0x8006DC30 | 0x8006DCE4 | 1 | 0x80036884 |
| 0x8006E26C | 0x8006E300 | 1 | 0x80036530 |
| 0x8006E588 | 0x8006EB24 | 3 | 0x80036C1C |
| 0x8006F004 | 0x8006F5B0 | 3 | 0x80036C34 |
| 0x8006F9A8 | 0x8006FA68 | 1 | 0x80036468 |
| 0x8006FE70 | 0x8006FF28 | 2 | 0x80036F0C |
| 0x80070388 | 0x80070428 | 2 | 0x80037180 |
| 0x80070950 | 0x800709CC | 2 | 0x800707CC |

**What they read, measured per site rather than assumed:** displacements `+0x18` (`lw`, the distance
H), `+0x20` (`lh`, OFX) and `+0x22` (`lh`, OFY) at **10 of 10** sites — the *same* offsets the
`0x80042910` publication reads. The base register varies (`a0` at six sites, `a2` at four); that
variance is reported by the tool rather than smoothed away. Their callers are all in one dispatch
region, 0x80036468–0x800372FC, and `FUN_80036430` reaches them under **bit flags in one global word**
(`s4[0x95B] & 4` for the object list, `& 8`, `& 0x10`, plus a mode byte at gameState+0x1CA8 that
also selects the geometry near plane, 0 or 2).

**And 55 `RTPS` sites** (sub-op 0x01, the framework's own name) are the guest's perspective
transform, statically present. The report states 158 of 214 COP2-looking words carry a sub-op the
framework's `gte_name()` table does not name, and refuses to fold those into a behaviour count: the
text extent holds data and ASCII, and `0x4A5F4349` at 0x80010108 is the ASCII `J_CI`, not an
instruction.

## 3. ROOT CAUSE OF "WIDENING IS INVISIBLE", which is not what the brief said

The guest writes `H` from **16 raw `ctc2 rX,$26` words plus 2 `jal SetGeomScreen` calls — 18 writers**
in 128,512 words. `ProjectionOwner` listens at `0x80042910`, reached by **2 of those calls** plus the
state-zero literal. It is a correct owner of that publication; it widens **176 of 176**.

**Ten of the remaining writers are the geometry submitters' own publication, immediately before their
own RTPS.** So a widening applied only at `0x80042910` is overwritten by the guest before a single
3D vertex is projected. The canvas grew 512 -> 684 and the picture did not, and the reason is
arithmetic, not mystery.

**THE SEAM.** The `ctc2 rX,$26` sites are interior labels inside a function body, and a label reached
by fall-through can never be an override key — the override would return to an `r31` the tail never
set. The ten *entries* are valid keys, but an entry-level override cannot help either: the guest
writes CR24/25/26 **inside** the body and then transforms, so the widened value is already gone by the
time an override regains control. The framework's per-Core GTE op observer fires immediately before
`GTE_Instruction` for every guest COP2 op (`lightrec_executor.cpp` `cop2Operation` -> `gte_op_at` ->
`GtePreOpObserver::observeAround`) and Lightrec exports the GTE registers back after the op, so a
value written in the pre-op callback is the value the transform consumes. `CtrGeometryProjectionOwner`
keys on that instant, which covers all 18 writers by construction rather than by a list a new
submitter could slip past.

## 4. WHAT LANDED

`game/video/geometry_projection_owner.{h,cpp}` and `ScopedGteProjectionObservation`, composed by
`CtrFrameDriver::stepFrame`. It reads the live triple, asks `ProjectionOwner` for the **same** plan
and the **same** `widenViewProjection` rule, and republishes through the framework's own
`libgte_set_geom_*`. The plan has one home: `ProjectionOwner` stores it, and the second owner reads
it, so the two cannot disagree.

Three things the first version got wrong, each caught by a mechanism rather than by reading:

1. **The owner was never bound to its plan.** `setProjectionOwner` was missing, so all 110,432
   projections returned "nothing to do" and the log said `0 widened` — which reads exactly like a
   title with no 3D geometry. The census now has a `refusedNoPlanOwner` counter so that specific
   wiring defect can never again be reported as a fact about the game. (Both were 0 after the fix.)
2. **The census did not sum.** It reported `911 widened` of 110,553 with no account of the other
   109,642, so the remainder read as an anomaly instead of as idempotence skips. The five outcomes
   (`widened`, `alreadyOwned`, `leftAtRetail`, `projectionsBeforePlan`, `refusedNoPlanOwner`) are now
   exhaustive and the report prints the sum against the total.
3. **`lastRetailDistance` recorded the owner's own value** on the next republication, so the number
   labelled "retail" was the widening. It is now captured before the write that overwrites it.
4. **The first gate asserted an eight-instruction prologue and the gate itself reported 0 of 10.**
   That is the instrument giving the other answer, on its own subject, and the real shape is five
   instructions.

## 5. MEASURED RESULT — both legs, from the shipping product

Both legs are the provisioned disc, 29,034 host fields each, 176 publications each, same fault PC.

| measurement | 4:3 | 16:9 |
|---|---|---|
| GTE ops offered while armed | 446,453 | 446,574 |
| perspective transforms consuming H | **110,432** | **110,553** |
| of those, **widened** | **0** | **911** |
| already carried the widened H | 0 | 109,642 |
| left at retail's H | 110,432 | 0 |
| retail H at the last widening | 320 | 320 |
| published H | 0 (never published) | **428** |
| `[wide]` LAST line `render_width` | 512 | **684** |
| fields / publications | 29,034 / 176 | 29,034 / 176 (identical) |

**The 3D prim count is no longer zero: 911 of 110,553 perspective transforms now run with the owned
plan's H (428) instead of retail's (320), in a leg where 4:3 widens 0 of 110,432.** The 4:3 leg
publishes nothing at all, so the shipping default is provably unchanged.

**Widening is visible in the picture, and it is re-projection, not a stretch.** The crate's faces stay
trapezoidal and the tunnel recedes correctly in the 16:9 capture.

| measurement | 4:3 | 16:9 |
|---|---|---|
| rightmost submitted `x1` | 812 | **894 (+82 columns)** |
| matched prims with a WIDER bounding box (frame,id) | — | **39,916 of 67,134 Gouraud (59.5%)** |
| matched prims identical / narrower | — | 14,536 / 12,788 |
| drawn band on the 960x720 sink | 960 | **719** (was **695** before this change, issue 0026) |
| right margin | 0 | **241 columns, 0/173,520 non-black (0.00%)** |
| left margin | 0 | 0 columns |

## 6. WHAT IS **NOT** ESTABLISHED, stated rather than implied

- **The widening is still a FAILED widening as a player-facing result.** 241 of 960 columns are black
  and 0.00% non-black. The band is no longer bounded by the same content: the 3D geometry reached 894
  submitted columns and the band moved 695 -> 719, but the right margin is **unchanged at 241
  columns**, which means the limit is now 2D content a horizontal projection change cannot reach.
  **The 2D backdrop and HUD need their own owner, and the next measurement is which prims produce the
  last non-black column** — `bg` and `tex` in the same CSV will answer it.
- **The 695 -> 719 comparison is a CROSS-RUN comparison.** 695 is issue 0026's recorded pre-override
  16:9 band; 719 is this change's. Same binary family, same disc, but two runs. The same-binary
  evidence that the override changed the picture is the census itself: 911 widenings with published
  H = 428 against 4:3's 0 and 320.
- **`H` is a COUPLED scalar in this title and the coupling is NOT yet accounted for.** There are
  **4** `cfc2 rX,$26` readers: 0x8006A6B8 stores `2H - CR7` into the scene descriptor at +0xF4,
  0x80070AF8 **divides by** H, 0x80070EE8 builds `4H`, 0x80071150 uses `H/2`. The previous recorded
  claim that "nothing in the 128,512-word text reads `H` back" was **false** and is corrected in
  `game/video/widescreen_owner.h`. The guest's own viewport math moves with the focal length, so a
  1.34x H does not give a 1.34x picture. **Whether that coupling is proportional (benign) or absolute
  (gameplay-visible) is NOT ESTABLISHED.** The simulation was not disturbed by this change — both legs
  are 29,034 fields, 176 publications, same fault address — but that is the simulation, not the
  guest's derived viewport.
- **The centre is deliberately NOT re-written.** `widenViewProjection` re-centres, and the two guest
  call sites scale the centre differently: PsyQ's `SetGeomOffset` at 0x8007782C does `sll a0,a0,16`,
  while all ten submitters do `sll v1,v1,15`. Both read displacement `+0x20`, so either they are the
  same field in two structs or the same field at two scales, and **the bytes do not settle which.**
  Applying the centring here would be a guess, so the owner reports the centre and leaves it. The
  readback assertion in the owner's own code is what caught this: the first version published
  `(342,120,428)` and read back `(11206656, 3932160, 428)`.
- **The 10 submitters are reached by 17 direct `jal` sites and 0 `j` targets out of 224 `jalr` sites
  the scan does not resolve.** An indirect caller would be invisible to this census; the `jal` column
  is a floor, not a total.
- **The run still faults at `0x8006C0FC` in field 29,033** (issue 0024). Nothing here reaches 3D
  gameplay; every number above is measured in the Naughty Dog attract sequence, which is 3D content
  and is the only 3D content the run reaches.
- **S005 is still `missing`.** The guest's 3D path is recovered and its projection is owned, but the
  capability is "frames produced by a game-state native renderer", and there is no native producer
  yet. This change makes the guest's own 3D visible and makes a native producer's camera input
  reachable; it does not substitute for one.

## 7. Falsifier

`tools/ctr_binary_probe.py gte-projection` fails, in CTest, if any of these stop holding on the
authenticated image: the shared 5-instruction tail at any of the ten sites; the descriptor
displacements at 10 of 10; the quoted first-submitter words; the four `H` readers. Its two negative
cases are the ones that matter: corrupting one tail word must make that site report a difference
while the other nine still pass, and filtering the four readers out must make the census report 0 — so
that the 4 above is a property of the image and not of the scan. `tests/ctr_geometry_projection_owner.cpp`
(23 checks) fails if the register numbers drift from the framework's writer, if a second perspective
transform through one publication compounds the widening, if a 4:3 plan publishes anything, or if the
five census outcomes stop summing to the projection count.
