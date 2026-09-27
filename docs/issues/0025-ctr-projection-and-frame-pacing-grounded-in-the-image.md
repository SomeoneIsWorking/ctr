# 0025 — CTR's projection publication and frame pacing, grounded in the provisioned image

`SCUS_944.26` is provisioned at `scratch/raw/ctr/SCUS_944.26`, SHA-256
`7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838`. Everything below is read out
of that file by `tools/ctr_binary_probe.py`, which imports psxport's shared PS-X EXE reader and
R3000A decoder rather than reimplementing either. Each command prints its own coverage.

**The workspace map's "no disc image exists on this machine" was wrong, and so was the VSync
reading built on it.** Both corrections are below, with the bytes.

## 1. Coverage, stated before any conclusion

| scope | covered | not covered |
|---|---|---|
| main executable | all 128,512 instruction words of `[0x80010000,0x8008D800)` | — |
| `BIGFILE.BIG` overlays | **0 of the archive's images** — `BIGFILE.BIG` is not provisioned | any address reached only from an overlay. CTR's overlays reuse load addresses, so an address is not an identity here |
| indirect calls | 224 `jalr` sites have no statically resolvable target | a call to one of these leaves through a register |

The `jalr` count is reported by every census below so the gap is a number, not a silence.

## 2. The projection publication — S004's core, now grounded

`0x80042910`, body `[0x80042910,0x80042974)`, quoted verbatim:

```
0x80042910: 27BDFFE8  addiu sp, sp, -24
0x80042914: AFB00010  sw s0, 16(sp)
0x80042918: 00808021  addu s0, a0, zero      ; s0 = the view descriptor the guest passed in $a0
0x8004291C: AFBF0014  sw ra, 20(sp)
0x80042920: 96020020  lhu v0, 32(s0)        ; width  = view+0x20
0x80042928: 00021400  sll v0, v0, 16
0x8004292C: 00022403  sra a0, v0, 16
0x80042930: 000217C2  srl v0, v0, 31
0x80042934: 00822021  addu a0, a0, v0
0x80042938: 96020022  lhu v0, 34(s0)        ; height = view+0x22
0x8004293C: 00042043  sra a0, a0, 1         ; a0 = width/2
0x80042940: 00021400  sll v0, v0, 16
0x80042944: 00022C03  sra a1, v0, 16
0x80042948: 000217C2  srl v0, v0, 31
0x8004294C: 00A22821  addu a1, a1, v0
0x80042950: 0C01DE0B  jal 0x8007782C         ; SetGeomOffset
0x80042954: 00052843  sra a1, a1, 1          ; a1 = height/2
0x80042958: 8E040018  lw a0, 24(s0)         ; H = view+0x18
0x8004295C: 0C01DE07  jal 0x8007781C         ; SetGeomScreen
0x80042960: 00000000  nop
0x80042964: 8FBF0014  lw ra, 20(sp)
0x80042968: 8FB00010  lw s0, 16(sp)
0x8004296C: 03E00008  jr ra
0x80042970: 27BD0018  addiu sp, sp, 24
0x80042974: 27BDFF80  addiu sp, sp, -128     ; the NEXT function's prologue: the body ends here
```

So the rule is `OFX = width/2`, `OFY = height/2`, `H = view+0x18`, and the whole projection
surface is three numbers. The three offsets in `ProjectionOwner` are no longer taken on trust.

The two leaves, quoted:

```
0x8007781C: 48C4D000  ctc2 a0, $26          ; SetGeomScreen -> GTE CR26 (H)
0x80077820: 03E00008  jr ra
0x8007782C: 00042400  sll a0, a0, 16
0x80077830: 00052C00  sll a1, a1, 16
0x80077834: 48C4C000  ctc2 a0, $24          ; SetGeomOffset -> GTE CR36 (OFX)
0x80077838: 48C5C800  ctc2 a1, $25          ;              -> GTE CR37 (OFY)
0x8007783C: 03E00008  jr ra
```

**Three direct callers, and they are the three return addresses already in
`native_ownership.h`:** `0x80024CCC` → `0x80024CD4`, `0x8003BD2C` → `0x8003BD34`,
`0x8003F5C0` → `0x8003F5C8`.

**A SECOND publication exists, and the docs did not record it.** State zero publishes a literal
projection directly, bypassing the descriptor:

```
0x8003C84C: 24040100  addiu a0, zero, 256   ; OFX = 0x100
0x8003C850: 0C01DE0B  jal 0x8007782C         ; SetGeomOffset
0x8003C854: 24050078  addiu a1, zero, 120   ; OFY = 0x78
0x8003C858: 0C01DE07  jal 0x8007781C         ; SetGeomScreen
0x8003C85C: 24040140  addiu a0, zero, 320   ; H  = 0x140
```

**Retail's startup projection is `OFX=256, OFY=120, H=320`.** `SetGeomOffset` and
`SetGeomScreen` therefore have TWO callers each, not one. Anything that widens by intercepting
only `0x80042910` leaves this one at retail, which is a latent inconsistency between the boot
frame and every frame after it.

## 3. The retail frame rate — 2 fields per game frame, and NOT via a VSync argument

### The wait semantics, read out of the image

`VSync` is `0x80075350` (`lui v0,0x8009`). Its argument branches:

```
0x800753A8: 04810005  bgez a0, 0x800753C0    ; a0 <  0 -> j 0x800754B4, return: NO WAIT
0x800753C4: 1082003A  beq a0, v0, 0x800754B0  ; a0 == 1 -> return:            NO WAIT
0x800753CC: 18800007  blez a0, 0x800753EC     ; a0 == 0 -> target is a SNAPSHOT of the field clock
0x800753F8: 00002821  addu a1, zero, zero
0x800753FC: 2485FFFF  addiu a1, a0, -1       ; a1 = a0 - 1
0x800754CC: 00052BC0  sll a1, a1, 15         ; the wait counts (a0-1) << 15
```

This confirms the workspace's established method on CTR's own bytes, and adds the detail the
method statement did not carry: **`VSync(n>=2)` waits `n-1` fields, not `n`.** An argument of
`0`, `1` or `-1` therefore carries **no** rate information.

### The census — 32 of 32 sites, argument resolved at 32 of 32

| argument | sites | what it is |
|---|---|---|
| `VSync(-1)` | 21 | field-clock query. No wait, no rate |
| `VSync(0)` | 6 | return current field. No wait |
| `VSync(2)` | 1 — `0x8003206C` | waits 1 field |
| `VSync(30)` | 4 — `0x8003C4C0`, `0x8003CF30`, `0x80078C9C`, `0x80078CE0` | waits 29 fields |

**Five sites wait a field count and not one of them is in the frame loop.** The frame owner's
only VSync is `0x80037878`, a conditional debug call whose delay slot is
`addu a0,zero,zero` — `VSync(0)`, no wait.

### What actually paces the game

The frame loop busy-spins on a **two-field countdown**, not on a VSync argument. The countdown
is `[gp+0x348]`, and exactly **four** instructions in the whole text touch it (scanned 128,512
words for a base-register-28 access with displacement `0x348`):

```
0x80037930: 24020002  addiu v0, zero, 2     ; the frame loop loads 2
0x80037934: AF820348  sw v0, 840(gp)        ; and arms the countdown
0x80034AEC: 2442FFFF  addiu v0, v0, -1      ; the callback decrements by exactly 1
0x80034AF0: AF820348  sw v0, 840(gp)
0x800378C0: 8F820348  lw v0, 840(gp)
0x800378C8: 1C40FFF2  bgtz v0, 0x80037894   ; the suffix spins until the countdown is not positive
```

The decrementer is the vertical-blank callback, installed at:

```
0x8003C8E8: 3C048003  lui a0, 0x8003
0x8003C8EC: 0C01DC95  jal 0x80077254         ; the callback installer
0x8003C8F0: 24844AA4  addiu a0, a0, 19108  ; a0 = 0x80034AA4
```

Three details make the tick unit a **field** rather than a guess:

1. the same callback increments `gameState+0x1CE0`, and the frame suffix reads that counter and
   tests it against 7 (`sltiu v0,v0,7` at `0x800378A4`) — the game itself uses the callback's
   tick as its field unit;
2. the installer `0x80077254` resolves its callee through **main RAM** at `0x8009C020`
   (`lui v0,0x8009; lw v0,-16352(v0); lw v0,20(v0); jalr ra,v0`) with the reason literal `4`.
   The occupant of that slot is written at runtime, so **which** function it is cannot be
   determined from the main image — stated rather than assumed;
3. the countdown is armed with a literal `2` and drained one per tick.

**Conclusion: 2 fields per game frame.** On the 60 Hz NTSC PlayStation video mode that is
**~30 game frames per second**. The 2-fields-per-frame half is measured from bytes; the 60 Hz
field rate is a hardware constant of the NTSC mode, not something this binary states.

### The correction this forces

The workspace map recorded "`ctr`: `VSync(2)` x1, plus 0 and -1 → **leads to 30 fps**, one site
only". **The reasoning was wrong even though the number is right.** That single `VSync(2)` site
is `0x8003206C`, inside `FUN_80031FDC`'s `param_5 == -1` branch — a boot resource load, not the
frame loop. The frame rate is 30 fps for an entirely different and now-measured reason: a
two-field countdown drained by the vblank callback.

**What this licenses about interpolation.** CTR is a **30 fps** title, so an interpolated 60 fps
presentation is *in scope* for G003 — the goal is not ruled out by the title's rate. It is still
not buildable: S007 needs S005, a native renderer that does not exist. **No interpolation path
was added**, and none should be until S005 lands.

## 4. The horizontal cull — and why it is NOT the projection H

The two main geometry paths cull like this:

```
0x8006E5C0: 24C3FFFE  addiu v1, a2, -2
0x8006E5C4: 1C600002  bgtz v1, 0x8006E5D0    ; a2-2 > 0 -> v1 = 0
0x8006E5C8: 24030000  addiu v1, zero, 0
0x8006E5CC: 24030002  addiu v1, zero, 2      ; a2 in {1,2} -> v1 = 2
0x8006E5D0: AC230054  sw v1, 84(at)          ; DAT_1F800054 = the near plane
```
and the test is `objectZ (+0xD8) - nearPlane < 1` → cull. `FUN_8006F004` repeats it at
`0x8006F04C`. Scanning the text for the `lui <reg>,0x1F80` + store-displacement form found 10
sites, of which these 2 are the near plane and 8 are register spills or unrelated output slots.

**The near plane is a literal `0` or `2`, chosen by the caller's third argument. It is not `H`.**
`H` reaches the GTE only, through `SetGeomScreen`'s `ctc2 a0,$26`, and nothing in the text reads
it back. So raising `H` **cannot** cull near geometry here — the opposite of Crash 1, where the
bound *was* `H` (`H < Z < 12000`).

**An encoding lesson worth recording.** My first scan for this near plane looked for
`lui 0x1F80` followed by an `addiu 0x0054`, and found **zero**. The address is reached by `lui`
plus a **store displacement** (`sw v1, 84(at)`), which that pattern cannot see. A scan that
returns 0 here is a scan of the wrong shape, not an absence.

### Gameplay reads of a projection scalar

The widening this title needs touches the three GTE control registers only. The guest's view
descriptor is **not written** by the new owner, so no gameplay read of it can be affected by
construction — the owner's test asserts the descriptor's three fields are byte-identical after a
widening publication, and a mutant that writes them turns that assertion red.

What I did **not** establish: a complete reader census of `view+0x18` / `+0x20` / `+0x22`. The
descriptor reaches at least two different bases (`gameState+0x168` from the lens-flare path at
`0x80024CCC`, and a computed pointer from `0x8003BD2C`), so a displacement scan is not
selective and a dataflow census over the whole text is the job, not a grep. **Reported as not
determined, with the reason.**

## 5. The owner written

- `game/video/view_projection_plan.{h,cpp}` — the pure rule: guest view facts + a
  `GuestProjectionPlan` → the published `(OFX, OFY, H)`. No `Core`, so it is directly testable.
  It reuses psxport's existing `guest_projection_plan` rather than adding a second widening
  formula.
- `game/video/projection_owner.{h,cpp}` — the seam-registered owner (already bound to
  `kProjectionProducer`). Order is deliberate: capture the guest's view, run the **retail** body,
  **compare against retail and abort on disagreement**, and only then apply the plan. So retail
  stays the oracle and a plan can never excuse a guest disagreement. The widening reaches the
  GTE through `libgte_set_geom_offset/screen`, the framework's one implementation of "publish a
  projection and record it", so the control registers and `ProjParams` cannot drift.

**Default behaviour is unchanged.** No plan latched, or a 4:3 plan, reproduces the retail triple
exactly — both cases are asserted. A 16:9 plan on a 320-wide view gives `OFX 160→214`,
`OFY 120` unchanged, `H 320→428`, with the guest descriptor untouched.

**Verified it can fail**, on copies in `scratch/`, never on the live tree: an owner that ignores
the plan fails the widening assertions; an owner that reuses the retail centre fails only the
`OFX` assertions; an owner that writes the guest descriptor fails the "guest's H word is
unchanged" assertion.

## 6. Instrument corrections made along the way

1. **The census walk was reading the wrong instruction.** It started one below the call, so it
   skipped the `jal`'s own **delay slot** — and on this title the argument is in the delay slot
   more often than before the call. It reported `VSync(1)` at `0x8003C4C0` where the delay slot
   `0x8003C4C4` is `addiu a0,zero,30`. Fixed to start at `site+4`; a selftest case now pins it.
2. **A fork-based dataflow walk was unsound** and produced plausible nonsense (`VSync(2148073472)`
   from a stray `lui`). Replaced with a single linear predecessor chain that crosses calls and
   stops at branches — sound, and it leaves a site UNRESOLVED rather than guessing.
3. **Ghidra's decompiled call order is not address order.** I mistrusted one site because of it
   and was wrong; the tool was wrong. The cross-check now compares per function and asserts the
   site count matches the argument count, so the correspondence is forced rather than assumed.
4. **`psx_exe` reports `gp=0`** for this executable, so the tool has no `gp` subcommand that
   prints an address. The frame-pacing scan keys on the *encoding* (base register 28, displacement
   `0x348`), which needs no `gp` and is complete.

## 7. Cross-validation

`tools/ctr_binary_probe.py selftest` — 22 checks, all passing, including the negatives: a census
with no matches must report zero *with its denominator* and name the `jalr` gap; the
argument-semantics guard must refuse a corrupted image; a non-PS-X-EXE and a missing image must
be refused, the latter naming the re-provision command. The VSync site list is found twice, by
the decoder and by a raw opcode-word scan, and the two must agree. The byte census is
cross-checked against an independent Ghidra decompilation census: **30 sites compared, 0
disagreements**; the 2 sites Ghidra's call-flow scan did not list (`0x80078C9C`, `0x80078CE0`,
both `VSync(30)`) are reported as byte-walk-only rather than quietly folded in.

## 8. Still open

- `S004` is grounded for the projection publication and its callers. The lens-flare producer
  `[0x80024C4C,0x80025138)` and the remaining raw GTE-control writes are still not attributed.
- `S005` has no producer, so **the widening cannot be seen**. The owner changes nothing visible
  until a native renderer exists; the honest status is a mechanism, not a capability.
- The second, literal projection publication at `0x8003C84C` is not yet owned. It should be.
- The reader census of the projection descriptor is not done (§4).
- The occupant of the runtime callback slot behind `0x80077254` is not determinable from the
  main image.
- **No `BIGFILE.BIG`.** Every result here is the main executable only.
