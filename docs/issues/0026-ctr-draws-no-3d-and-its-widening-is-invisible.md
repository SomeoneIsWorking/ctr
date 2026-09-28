# 0026 — CTR draws no 3D at all: the census that says so, and a widening that is now real but invisible

Every number below is a measurement with a denominator, read out of `scratch/raw/ctr/SCUS_944.26`
(bytes) or out of a headless run of the shipping product against the provisioned disc. Where a
number could not be obtained, this issue says so instead of printing a zero.

## 0. The premise this issue was written under was wrong, and that is the first finding

The task that opened this work said CTR "shows nothing on screen, because nothing produces a frame".
**CTR does present frames, and has done so all along.** `PSXPORT_PRESENT_SHOT_AT=50` captures the
attract sequence — a Naughty Dog logo crate under a warp tunnel — at **99.43% non-black
(687,244 of 691,200 pixels)**. The first present in a run *is* black (frame 1 measured 0 of
691,200), which is what `docs/project-state.md` S009 recorded as "a completed black output"; that
statement is true of the FIRST present and false of the run.

What is still true is that there is no **native** producer. The product's own log says so:

    [render] render path = gte — geometry from the GUEST (its own GTE + ordering table),
             rasterized by the PC rasterizer (SDL_GPU), PC enhancements LOCKED OUT

`CtrRuntime::renderCapabilities()` declares `defaultPath = RenderPath::Gte`,
`nativeRenderPath = false`. So S005 remains `missing` — correctly — and the correction is only to
the reason, not the state.

## 1. What CTR actually draws, and the census that says how much of it is 3D

`PSXPORT_PRIMDUMP=1:3000` over the whole reachable window:

| scope | prims | 3D | ops |
|---|---|---|---|
| present frames 1..191 (the tool declared the range and the flush) | **73,695** | **0** | `0x30`×12478, `0x34`×38606, `0x36`×7607, `0x68`×14300, `0x38`×522, `0x2A`×34, `0x2E`×148 |
| present frames 192..3000 | **0** | 0 | — the run faults first |

**Zero of 73,695 primitives carry 3D.** Every one is a flat 2D polygon or sprite. This is the
answer to "what does CTR draw", and it is why no native 3D producer can be justified: there is no
3D geometry in the reachable window for one to consume, and per the workspace's own rule a producer
that replays the guest's 2D ordering table is the **banned** guest-geometry fallback.

**Coverage, stated.** `PSXPORT_PRIMDUMP` takes ONE frame or a RANGE, not a comma list. A first
attempt passed `5,10,15,…,195` and armed **only frame 5** (`atoi("5,10,…")` == 5) while the log
line said `armed frames -1..-1`; the tool's own flush line then reported 3 of 22 requested frames.
Read as "the other frames drew nothing", that would have been a confident wrong answer. The numbers
above use the `a:b` range form and the tool's declared range is quoted.

**Where the run dies.** `0x8006C0FC` in field 29,033, the frontier already recorded in
`docs/issues/0024` — the duplicate `0x8006BF30` render path with the corrupt list pair. That is
S009's work, not this issue's, and it is upstream of every 3D primitive.

## 2. The projection census — the instrument that turned a silence into a number

`ProjectionOwner::publish` speaks only on a **disagreement**, so before this work a run in which the
owner never fired printed nothing, and "no `ctr-projection` line" was indistinguishable from "the
owner was never reached". That is the question S004 and S006 both rest on. `ProjectionOwner` now
carries a per-source census with a **field** denominator, reported on the fault path (the run that
needs the number most is the one that dies):

    publication census over 29034 host field(s): 176 descriptor publication(s) at 0x80042910
    (lens-flare 0 = 0x80024CD4, state-zero 1 = 0x8003BD34, overlay 175 = 0x8003F5C8),
    0 widened, 0 literal startup publication(s) at 0x8003C84C.

**Pre-GTE camera state IS reachable — 176 publications.** That is the load-bearing positive result:
a native producer's camera input is not blocked. The three sources are kept apart because they are
not interchangeable, and the **lens-flare producer `[0x80024C4C,0x80025138)` never runs in this
window: 0 of 29,034 fields.**

## 3. The literal `0x8003C84C` publication: it runs, and it CANNOT be owned there

Issue 0025 recorded this site and said "it should be" owned. **It cannot be, at that address.**

    0x8003C84c: 24040100  addiu a0, zero, 256   ; OFX = 0x100
    0x8003c850: 0c01de0b  jal 0x8007782C        ; SetGeomOffset
    0x8003c854: 24050078  addiu a1, zero, 120   ; OFY = 0x78
    0x8003c858: 0c01de07  jal 0x8007781C        ; SetGeomScreen
    0x8003c85c: 24040140  addiu a0, zero, 320   ; H   = 0x140

A full-text scan of all 128,512 words for `j`/`jal` to this address finds **0 sites of each**. It is
reached by **fall-through** from `0x8003C848`, inside the state-zero main at `0x8003C58C` (`kGuestMain`,
whose only direct caller is `0x800779D8`). A fall-through label is never a valid override key: the
override would return to an `r31` the tail never set. Owning it properly means owning the whole
0x8003C58C body, or intercepting the two libgte **leaves** and keying on the return address
(`0x8003C854`/`0x8003C85C` vs the descriptor route's `0x80042954`/`0x80042960`).

**It does run**, measured without owning it: the GTE triple left behind is sampled at the first
descriptor publication, *before* the retail body, and reads

    GTE triple left by boot, sampled at the first descriptor publication before retail ran:
    (256,120,H=320), valid=1

which is retail's literal triple exactly. A match is positive evidence the site ran; a mismatch
would have been positive evidence it did not. Neither is an inference from "it is straight-line
code".

**Decision: NOT owned in this task, and the reason is the measurement, not the cost.** The
boot publication is boot-only, and §1 shows the reachable picture contains **zero 3D primitives**,
so a change to `H`/`OFX`/`OFY` provably cannot move a presented pixel while the 3D path faults.
Adding two overrides on the shared libgte leaves — with a double-publish hazard inside the already
validated descriptor route — would buy a correct GTE triple that is, today, invisible. The exact
fix is named above and is unblocked the moment 3D geometry exists.

## 4. CTR's view is 512x216, and that is not the number anyone assumed

The widescreen owner latches its plan from the extent **the guest's own publication carried**
(the view descriptor's `+0x20`/`+0x22` — the same two words `0x80042910` reads). Two independent
measurements agree that CTR is 512 wide, not the 320 an NTSC assumption gives:

* the guest's GP1(08) display mode is `0x08000002`, and `gp1_display_width` decodes `mode & 3 == 2`
  as **512** dots (`s_disp_w` = 512, confirmed by the product's own `[wide]` line);
* the boot literal `OFX = 256` and the measured rule `OFX = width / 2` (`0x8004293C sra a0,a0,1`)
  pin the view descriptor at **512** wide, and the live owner reports the view as **512x216**.

**This was my own error, twice, and both are worth recording.**

1. The first owner latched on field 0 and hardcoded 320x240. `s_disp_w` read **320** on that field
   and **512** at the first present. A plan built from 320 is 428 wide against a 512-wide picture —
   **narrower than the native extent** — so `present_display_width` correctly declined to call it a
   widening, and the two legs came out byte-identical. Removing the boot-time latch and deriving the
   plan per publication deleted the whole class of problem: there is no longer a window in which
   the plan can be derived from a stale extent.
2. The test then caught a **real defect in the owner**: `widenViewProjection` compares the plan's
   projection extent with the plan's *own* native extent, so a stale-320 plan still read as a
   widening and would have been **published as a narrowing**, squashing the picture. The owner now
   refuses any plan not wider than the guest's own view. `tests/ctr_widescreen_owner.cpp` pins it.

**Root cause of CTR not widening at all, measured:** `CtrRuntime::guestWidescreenProjection()`
returned the base `nullptr`, so `gpu_vk_latch_guest_projection` always resolved
`requested = Standard4x3` and every plan was 4:3 whatever the settings file said. `CtrWidescreen`
now answers the framework's question and is bound through that override.

## 5. The picture pair, and it is a FAILED widening

Two legs, each with **its own tracked** `PSXPORT_SETTINGS` (`tools/agent_settings_4x3.ini`
`aspect=0`, `tools/agent_settings_16x9.ini` `aspect=1`), launched through psxport's
`agent_environment`. The LAST `[wide]` line is quoted, never the first — `picture_announce` prints
on CHANGE and the 16:9 leg emits **two** lines:

| leg | `[wide]` lines | native_width | render_width | GTE widened |
|---|---|---|---|---|
| 4:3 | 1 | 512 | **512** | **0** of 176 |
| 16:9 | 2 (`…512` then `…684`) | 512 | **684** | **176** of 176 |

**The mechanism is real and the picture did not change.** Margin census, per leg, on 960x720 sink
captures (`tools/ctr_widescreen_pair.py`):

| leg | frame | drawn band | left margin | right margin |
|---|---|---|---|---|
| 4:3 | 50 | 960 col | none | none |
| 4:3 | 100 | 960 col | none | none |
| 16:9 | 50 | **695 col** | 24 col, **0/17280 non-black (0.00%)**, 1 distinct colour, 23/23 columns repeat their neighbour, uniform | 241 col, **0/173520 non-black (0.00%)**, 1 distinct colour, 240/240 repeat, uniform |
| 16:9 | 100 | **690 col** | 29 col, **0/20880 non-black (0.00%)**, 1 distinct colour, 28/28 repeat, uniform | 241 col, **0/173520 non-black (0.00%)**, 1 distinct colour, 240/240 repeat, uniform |

`render_width` grew 512 → 684 and the GTE triple widened on all 176 publications, yet **the drawn
band SHRANK from 960 to 695 columns and both margins are 0.00% non-black, one colour, uniform.** The
16:9 leg is the same 4:3 scene in a wider box. Verdict: **FAILED**, reported as one. This is the
Tekken 3 card shape, reproduced on a different title.

**Why, and it is not a presentation bug:** a widened projection can only add horizontal field if
something is drawn through it. §1 measured **0 of 73,695** prims as 3D, so nothing in the reachable
window consumes the widened `H`. The owner is correct and the scene has nothing to re-project.

**Stated limit of the instrument.** A wider canvas with the 4:3 picture *stretched* into it also
grows the drawn band, so the band test alone cannot separate a stretch from a re-projection. The
selftest pins that limit as a positive case rather than implying the tool can certify the non-black
case; the picture pair remains a human judgement.

## 6. The simulation was not disturbed

Guest-execution telemetry, both legs, from the same product run:

| measurement | 4:3 | 16:9 | |
|---|---|---|---|
| host fields | 29,034 | 29,034 | **identical** |
| descriptor publications | 176 | 176 | **identical** |
| fault | `0x8006C0FC` field 29,033 | `0x8006C0FC` field 29,033 | **identical** |

Widescreen is a presentation change, and it did not move the simulation by one field, one
publication, or one fault address. The widening reaches the GTE control registers only; a test
asserts the guest's view descriptor is byte-identical after a widened publication.

## 7. Instrument corrections made along the way

Each of these produced a **confident wrong answer** first, and each is the class the workspace has
already paid for.

1. **A decoder called with its arguments reversed** (`decode(word, addr)` instead of
   `decode(addr, word)`) printed a whole function as `lb v1, offset(zero)`, and the site scan that
   used it reported "**0** direct `j`/`jal` sites reach `0x8003C84C`" — a zero that happened to be
   the right answer for entirely the wrong reason. Re-run correctly, it is still 0, and now the
   zero means something.
2. **`PSXPORT_PRIMDUMP` given a comma list** armed one frame (§1).
3. **The widescreen pair tool wrote both legs' captures to the same filenames**, so the 16:9 leg
   overwrote the 4:3 leg's and the comparison censused one file twice. It printed a confident
   "the two legs are byte-identical" on a pair whose pictures plainly differ: an instrument
   confirming its own input.
4. **A mistyped dictionary key.** The leg table's key was `"16x9"` while every lookup said
   `"16:9"`, so the wide leg was never reported, the tool printed `UNMEASURED`, and the same output
   printed a sorted key list containing `'16x9'` — which reads as though the leg was there. The
   leg identities are now declared once, and the selftest asserts the table, the names, the
   settings files and the file's own source all agree.
5. **A margin census derived from the non-black bounding box is black by construction.** The first
   version tried to build a "partly drawn margin" and silently measured a 2-column margin instead of
   the 20 it asserted. The verdict was changed to compare the **drawn band** across legs, which is
   the measurement that can actually fail, and the impossible case was replaced by what is
   measurable.

## 8. Still open

- **S005 is still `missing` and cannot be closed here.** The precondition is measured: there is no
  3D geometry in the reachable window. A native producer would have nothing to draw, and the only
  geometry present is the guest's 2D ordering table, which is a banned source. The blocker is
  `0x8006C0FC` (issue 0024) and it is upstream of the first 3D primitive.
- **The lens-flare producer `[0x80024C4C,0x80025138)` is unattributed and never runs** (0 of 29,034
  fields). It is the largest measured producer body in the main image and the obvious next
  pre-GTE target, but nothing in this window reaches it, so its geometry is **not** determined.
- **The literal `0x8003C84C` publication is measured and unowned**, with the mechanism named (§3).
- **The reader census of the projection descriptor is still not done** (0025 §4). The owner's own
  test asserts the three descriptor words are unchanged after a widening, which is a construction
  guarantee rather than a reader census.
- **No `BIGFILE.BIG`.** Every byte-level result here is the main executable only.
- **S007 stays unstarted.** The 30 fps rate is measured, so interpolation is in scope, but S005 is
  its prerequisite and no interpolation path was added.
