---
id: 30
title: CTR's 2-fields-per-frame cadence comes from a vblank-drained countdown, not from its single VSync(2), and the workspace map's reasoning for it was wrong
status: open
symptom: The workspace map recorded "`ctr`: `VSync(2)` x1, plus 0 and -1 -> **leads to 30 fps**, one site only" and left the title's lerp scope undecided on that reasoning. `docs/issues/0025` had already measured the rate but recorded the correction only there, so the map still carried the wrong causal story.
tags: cadence,vsync,frame-rate,presentation,interpolation,lerp,bytes
created: 2026-09-28
updated: 2026-09-28
---

## Answer

**2 fields per game frame = 30 game frames per second. NOT 60 fps.** The number was already
right in `docs/issues/0025`; what this issue fixes is that the map's *reasoning* was wrong, and
that the number now has a registered instrument rather than a hand-run command.

**The `VSync(2)` the map cited is not the frame rate.** It is `0x8003206C`, inside
`FUN_80031FDC`'s `param_5 == -1` branch — a boot resource load:

```
8003206C  0C01D4D4  jal   0x80075350     ; VSync
80032070  24040002  addiu $a0,$zero,2   ; VSync(2) -- one field
```

The frame loop's **only** VSync is `0x80037878`, and its delay slot is
`addu $a0,zero,zero` — `VSync(0)`, which does not wait and carries no rate information.

## What actually paces the game

A two-field countdown at `[gp+0x348]`. **Scanned 128,512 words for a `$gp`-relative access to
`+0x348`; matched exactly 4, and the tool refuses at any other count** — a fifth access is how a
60 fps mode would hide, by arming the countdown from a second place.

```
0x80037930  24020002  addiu $v0,zero,2   ; the frame loop loads 2
0x80037934  AF820348  sw    $v0,840(gp)  ; and arms the countdown
0x80034AEC  2442FFFF  addiu $v0,$v0,-1   ; the vblank callback decrements by exactly 1
0x80034AF0  AF820348  sw    $v0,840(gp)
0x800378C0  8F820348  lw    $v0,840(gp)
0x800378C8  1C40FFF2  bgtz  $v0,0x80037894  ; the suffix spins until it is not positive
```

## The `n >= 2` rule, established on CTR's own bytes

Not inherited. `ctr_binary_probe.py vsync-wait-semantics` reads each word back and refuses on a
mismatch:

```
0x800753A8: 04810005  bgez a0  -> a0 < 0 no wait
0x800753C4: 1082003A  beq a0,v0(=1) -> a0 == 1 no wait
0x800753CC: 18800007  blez a0  -> a0 <= 0 snapshot target, no field wait
0x800753FC: 2485FFFF  addiu a1,a0,-1 -> a0 >= 2 waits (a0-1) fields
0x800754CC: 00052BC0  sll a1,a1,15    -> the count is a field count
```

The helper returns immediately when `slt counter,target` is false, so count 0 does not wait.
**Only `a0 >= 2` is a field count; 0, 1 and negative carry no rate information.**

## The census, 32 of 32 sites with a denominator

`VSync(-1)` x21 (field-clock query) · `VSync(0)` x6 · `VSync(2)` x1 · `VSync(30)` x4.
**5 sites wait a field count and not one of them is in the frame loop.** The `VSync(30)` sites
wait 29 fields each and are timeouts.

The census covers only the main executable: **0 of CTR's `BIGFILE.BIG` overlays are
provisioned**, and 224 `jalr` sites have no statically resolvable target. Both are printed with
the counts, not hidden. An overlay-frame-rate change would not be visible here.

## Instrument

`tools/re_cadence.py`, registered as `ctr_cadence_selftest` and `ctr_cadence`. It reuses
`ctr_binary_probe.py` for the PS-X EXE reader, the R3000A decoder, the identity assertion and
the VSync semantics rather than adding a second implementation. `--selftest` is **7/7**, and each
negative was shown red:

| negative | shown red as |
|---|---|
| the countdown's arming literal broken | `0x80037930 is 0xDEADBEEF, not 0x24020002` |
| the per-field decrement broken | `0x80034AEC is 0xDEADBEEF, not 0x2442FFFF` |
| the frame suffix's block broken | `0x800378C8 is 0xDEADBEEF, not 0x1C40FFF2` |
| the boot-path `VSync(2)` site broken | `0x8003206C is 0xDEADBEEF, not the jal the census records` |
| the frame loop's `VSync(0)` delay slot broken | `is no longer VSync(0) and the 'not a field count' claim must be redone` |
| a **fifth** countdown access added | `has 5 accesses, not the 4 this cadence rests on` |

The last one is the discriminator that matters: a 60 fps CTR would arm the same countdown with
a 1 from somewhere else, and the census is 4-wide precisely so that has to be visible. It
required a substitution view rather than the probe's poison-value view, because a poison word
is not an access at all and the census would correctly ignore it.

## What this changes

CTR is a **30 fps title**, so an interpolated 60 fps presentation is **IN scope** for G003. That
was already the conclusion in `0025`; what changed is that it no longer rests on a hand-run
command or on a map entry whose stated reason was wrong.

## Falsifier

If a **fifth** access to `[gp+0x348]` exists in the main image, or one of the four is not the
word this issue quotes, `ctr_cadence.py` refuses and the 2-fields-per-frame claim falls with it.
That is the whole claim, and it is a four-instruction footprint.

Second falsifier: if a provisioned `BIGFILE.BIG` overlay arms the countdown with **1** for
gameplay frames, CTR is a 60 fps title and must receive **no** frame-rate change at all. The
tool cannot see this today and says so; the falsifier becomes checkable the moment overlays are
provisioned.
