---
id: 32
title: The content bounding the right margin is 14,822 untouched 2D primitives, and one class got NARROWER
status: open
symptom: The 16:9 leg draws 719 of 960 columns; the 241-column right margin is 0/173,520 non-black
state_items: S005, S006
tags: ctr,widescreen,gp0,census
created: 2026-09-29
updated: 2026-09-29
---

## What the state said, and what it did not name

`docs/project-state.md` S005 records the gap honestly: the drawn band is 719 of 960 columns, the
241-column right margin is `0/173,520` non-black, **the margin did not change when the 3D widened**,
and therefore "the limit is now 2D content that a horizontal projection change cannot reach".

That is a correct conclusion attached to an unnamed referent. "2D content" is a category, not a
measurement, and a category cannot be searched for. This issue names it, with denominators.

## Measured: the per-GP0-class census, both legs

Classified by **the GP0 command byte**, never by the `is3d` column. That column is the dead tap the
workspace map already records — its only writers have no callers, so it reads 0 for every title on
Lightrec and a census built on it measures nothing. (`op` in the capture is hexadecimal.)

    4x3 rows 73,695      16x9 rows 67,282

| op | 4:3 n | 16:9 n | coordinates identical | max x1 4:3 | max x1 16:9 |
|---|---|---|---|---|---|
| `0x2A` | 34 | 34 | yes | 0 | 0 |
| `0x2E` | 148 | 148 | **no** | 382 | 424 |
| `0x30` | 12,478 | 10,961 | **no** | **812** | **785** |
| `0x34` | 38,606 | 34,556 | **no** | 585 | 595 |
| `0x36` | 7,607 | 6,761 | **no** | 733 | **894** |
| `0x38` | 522 | 522 | **yes** | 512 | 512 |
| `0x68` | 14,300 | 14,300 | **yes** | 512 | 512 |

## Finding 0 — the two legs ARE the same 191 frames, so everything below compares like with like

Checked before relying on any of it, because a coordinate-identity claim across two captures is worth
nothing unless both captures are the same moments in the game.

    4x3   191 distinct frame(s), range 1..191
    16x9  191 distinct frame(s), range 1..191     same frame set: yes, 191 of 191 overlap

Every GP0 class also appears in the **same number of frames** in both legs (`0x30`, `0x34`, `0x36`,
`0x38` and `0x68` in 174 frames each; `0x2A` in 17; `0x2E` in 12). So both captures are the same 191
presented frames of the same attract sequence, and the coordinate identity below is a real comparison
rather than two unrelated scenes that happen to share a maximum.

**A vacuous check, caught in the act.** The first version of this comparison filtered on the op string
`"0x38"` while the capture stores bare hex `"38"` — so it matched **nothing**, and printed
`per-frame counts identical: True` for both untouched classes. A filter that matches no rows satisfies
every equality you ask of it. That is the seventh dead tap this workspace has met and the first one
this session's own new analysis produced; it is recorded here because the output looked exactly like
the confirmation it was not.

## Finding 1 — the margin's content is named, and proven untouched per frame

**`0x38` (522 prims) and `0x68` (14,300 prims) have byte-identical coordinates in both legs and stop
at exactly `x1 = 512`.** Together that is **14,822 of 67,282 submitted primitives — 22.0%** — and the
identity check is a stronger statement than a shared maximum: every one of their `x0,y0,x1,y1`
quadruples matches between 4:3 and 16:9, across all 191 frames, and so does their **per-frame count**
(`True` for both, over 174 frames each, against `False` for `0x30`, `0x34` and `0x36`). The widening
provably did not touch them.

`0x68` reaches off-screen to `x0 = -1024`, so it is a scissored or clipped draw class rather than a
bounded one, and that is why its coordinates can be identical while its visible extent does not move.
**That is the concrete referent for "2D content a projection change cannot reach": two GP0 classes,
22.0% of the frame's primitives, derived from something other than the horizontal projection.**
Issue 0031 already narrowed the 3D submission path to ten `jal`-reachable submitters; this is the
other half of the frame, and it has no equivalent owner yet.

## Finding 2 — one class got NARROWER, and 8.7% of the prims vanished

This is not in any prior record and it is not what a projection change should do.

* **Submitted primitives fell from 73,695 to 67,282 — 6,413 fewer, 8.7%** — and this is now a
  **per-frame** drop, not a capture artifact. Both legs cover the same 191 frames, and **174 of those
  191 frames submitted fewer primitives in 16:9**. The loss concentrates in `0x30` (−1,517) and `0x34`
  (−4,050), which together account for 5,567 of the 6,413.
* **`0x30`'s rightmost vertex moved LEFT, 812 → 785**, while `0x36`'s moved right, 733 → 894. Two
  classes in the same frame, over the same 191 frames, under the same widening, moved in OPPOSITE
  directions. Both classes also have per-frame counts that differ between legs, so this is not one
  anomalous frame.

A horizontal projection change scales vertex X about a centre. A class whose maximum X *decreases*
is either being clipped against a bound that did not move, culled by a visibility test whose
threshold changed, or authored from a value the widening also altered. The class count dropping by
8.7% at the same time is consistent with the first two.

**This is a behaviour change beyond presentation, and it is not yet explained.** It does not by itself
make the widening wrong — culling that tracks the new projection can legitimately remove offscreen
polygons — but it must be accounted for before the 16:9 leg can be called a pure re-projection, and
it is a different question from the margin.

## What this does NOT establish

* **Nothing here says what computes `x1` for `0x38`/`0x68`.** The state already records that the
  guest's derived viewport moves with focal length and that `H` has 4 measured readers in this title,
  which makes `H` the obvious suspect — but a suspect is not a writer, and no writer is named here.
* **The drop is established as per-frame but not as a defect.** Culling that tracks a widened
  projection can legitimately remove off-screen polygons, and nothing here shows the removed prims were
  ones that *should* have been drawn. What is established is that the 16:9 leg is **not** the 4:3 leg
  with only its vertex X rescaled: it submits 8.7% fewer primitives, from classes that differ
  per-frame, in two directions at once.
* The margin census (719/960 columns, 0/173,520 non-black) is quoted from the existing capture and is
  not re-measured here.

## The next step, named

1. **Find what writes `x1` for `0x38` and `0x68`.** Both stop at 512 while every projected class moves
   and both are per-frame identical across legs, so they are computed from a different owner than the
   projection. If that owner is the guest's `H`, the title needs the treatment the 3D path got in issue
   0031 — recovered into readable C++ with the byte evidence.
2. **Explain `0x30`'s leftward movement and the 8.7% per-frame drop**, or establish that the removed
   prims were legitimately off-screen. Until then the 16:9 leg cannot be called a pure re-projection.
3. Record the frame count in the primdump capture itself. Both legs happened to cover 191 frames here,
   and that equality is what made every comparison above valid — it was luck, not a guarantee, and the
   next capture may not line up.

## Falsifier

Each of these would make finding 1 wrong, and each is a measurement rather than an opinion:

* If a re-capture of the 16:9 leg over a **different** frame budget shows `0x38`/`0x68` coordinates
  changing, then "untouched" was an artifact of two captures that happened to align. The 191-frame
  equality is what rules that out **for this pair of captures**, and nothing rules it out for the next.
* If a scene with different content is censused, the coordinate identity no longer implies the widening
  left the class alone.
