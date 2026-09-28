#!/usr/bin/env python3
"""ctr_widescreen_pair.py — measure CTR's 4:3-vs-16:9 pair, and say whether the widening is real.

Every title in this workspace that owns a widening mechanism has, at some point, published a pair of
pictures that were the same picture in a bigger box. Tekken 3's card was re-centred into a wider
frame with both margins 0.0% non-black. A margin census WITH A DENOMINATOR is the only thing that
tells the two apart, and this tool exists so that claim is a measurement rather than an opinion.

WHAT IT MEASURES, per leg, each with its OWN tracked settings file:

  * the product's own `render_width` / `native_width`. The LAST `[wide]` line, never the first:
    `picture_announce` prints on CHANGE, so a leg that widens late prints 512 first and 684 second,
    and quoting the first reads "not widened" on a leg that is. The occurrence COUNT is reported so
    a one-line leg is distinguishable from a two-line leg.
  * a margin census with a denominator: for each side, how many pixels, how many non-black, the
    non-black SHARE, the number of distinct colours, and how many of its columns are exact repeats
    of their neighbour. A black margin is one repeated column; a re-centred 4:3 picture in a wide box
    is two of them, and that is the shape this tool is built to catch.
  * the guest-execution telemetry, compared between the legs. Widescreen is a presentation change, so
    the simulation it runs against must be identical; a difference here means the widening reached
    into guest state and the claim is void whatever the picture looks like.

THE VERDICT IS THREE-WAY, and the third state is the point:
  WIDENED     render_width > native_width AND the margins carry content.
  NOT-WIDENED the request produced the retail width. A no-op knob.
  FAILED      the width grew and the margins are black or repeated: the same 4:3 picture in a bigger
              box. This is a FAILED widening and is reported as one.

`--selftest` is the in-CTest self-check and needs NO disc, NO game and NO PIL: it exercises the
census arithmetic on synthetic pixel arrays, including the negatives — a black margin, a
re-centred-in-a-bigger-box picture, and a genuinely re-projected frame must all be told apart, and a
census that cannot say "FAILED" is not trusted to say "WIDENED".
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PSXPORT_TOOLS = ROOT / "external" / "psxport" / "tools" / "port"
sys.path.insert(0, str(PSXPORT_TOOLS))

from launch_environment import agent_environment  # noqa: E402

# THE LEG IDENTITIES, DECLARED ONCE. They were hand-typed in three separate places and one of them
# read "16x9" while every lookup read "16:9", so the wide leg was never reported, the tool printed
# UNMEASURED, and the same line printed a sorted key list that LOOKED like the leg was present. A
# self-inconsistent dictionary key is the cheapest possible way to make a tool confident and wrong.
LEG_NAMES = ("4x3", "16:9")
LEG_SETTINGS = {
    "4x3": ROOT / "tools" / "agent_settings_4x3.ini",
    "16:9": ROOT / "tools" / "agent_settings_16x9.ini",
}

# A pixel at or below this level in every channel counts as black. The framework's own present-shot
# uses the same threshold idea ("> 8 -> drawn"), so a margin this tool calls black is one the
# presenter's own coverage metric would also call undrawn, and the two cannot disagree.
BLACK_LEVEL = 8

WIDE_RE = re.compile(r"native picture: aspect=(-?\d+) wide_engine=(-?\d+) native_width=(\d+) render_width=(\d+)")
# The guest-execution telemetry compared between legs. These are the numbers widescreen must NOT
# move: a widening changes the GTE control registers, never the guest's own instruction or block
# counts. A difference in any of them means the two legs did not run the same simulation.
TELEMETRY_PATTERNS = {
    "fields": re.compile(r"publication census over (\d+) host field"),
    "publications": re.compile(r"host field\(s\): (\d+) descriptor publication"),
    "guest_blocks": re.compile(r"translated (\d+) block"),
    "guest_instructions": re.compile(r"executed (\d+) instruction"),
    "fallback": re.compile(r"fallback[^0-9]*(\d+)"),
}


# ---- the census, as pure arithmetic on RGB rows so it needs no image library ------------------------


@dataclass
class Margin:
    """One side of the drawn picture, counted against a denominator."""

    name: str
    columns: int
    pixels: int
    non_black: int
    distinct_colours: int
    repeated_columns: int
    uniform: bool

    @property
    def non_black_share(self) -> float:
        return 0.0 if self.pixels == 0 else self.non_black / self.pixels

    @property
    def all_black(self) -> bool:
        return self.pixels > 0 and self.non_black == 0

    def line(self) -> str:
        return (
            f"{self.name:<5} {self.columns:>4} col(s), {self.non_black:>7}/{self.pixels:<7} non-black "
            f"({self.non_black_share * 100:6.2f}%), {self.distinct_colours:>5} distinct colour(s), "
            f"{self.repeated_columns}/{max(self.columns - 1, 0)} comparable column(s) an exact repeat of "
            f"their neighbour, uniform={self.uniform}"
        )

    @property
    def columns_count(self) -> int:
        return self.columns


@dataclass
class PictureCensus:
    width: int
    height: int
    left: Margin
    right: Margin
    drawn_columns: int
    band_start: int
    distinct_colours: int
    fully_black: bool

    @property
    def centring_offset(self) -> int:
        """Black columns before the drawn band starts. A picture re-centred into a wider canvas has
        a POSITIVE offset; one that genuinely grew to its new edge has zero. This is the number that
        says "same picture, bigger box" in one integer, where the margin census cannot: a margin taken
        from the non-black bounding box is black BY CONSTRUCTION and so is always 0.0% non-black."""
        return self.band_start

    @property
    def margins_are_blank(self) -> bool:
        return self.left.all_black and self.right.all_black

    @property
    def uniform_margins(self) -> bool:
        """Every non-empty margin is ONE flat colour across the whole margin.

        A black bar is uniform; content is not. This is the property that separates "the canvas grew
        and the scene filled it" from "the canvas grew and nothing was drawn in the new part", and it
        is asked of the MARGIN rather than of the drawn band -- the drawn band's own width says the
        same thing more directly (see `verdict`), and both are reported.
        """
        margins = [m for m in (self.left, self.right) if m.columns > 0]
        return bool(margins) and all(m.uniform for m in margins)


def census_from_rows(rows: list[list[tuple[int, int, int]]]) -> PictureCensus:
    """Census an RGB image held as rows. `rows[y][x] == (r, g, b)`.

    Drawn extent is the bounding box of non-black pixels, which is what separates "the picture grew"
    from "the picture is the same size with black beside it": a pillarboxed frame has a NARROWER
    drawn box than a re-projected one, and both have a wider canvas.
    """
    if not rows or not rows[0]:
        raise ValueError("an empty image has no census")
    height = len(rows)
    width = len(rows[0])
    for row in rows:
        if len(row) != width:
            raise ValueError("a ragged image has no census: row widths differ")

    def is_drawn(pixel: tuple[int, int, int]) -> bool:
        return any(channel > BLACK_LEVEL for channel in pixel)

    min_x, max_x = width, -1
    colours: set[tuple[int, int, int]] = set()
    non_black_total = 0
    for row in rows:
        for x, pixel in enumerate(row):
            colours.add(pixel)
            if is_drawn(pixel):
                non_black_total += 1
                min_x = min(min_x, x)
                max_x = max(max_x, x)

    fully_black = non_black_total == 0
    left_end = min_x if not fully_black else width
    right_start = (max_x + 1) if not fully_black else 0

    def measure(name: str, xs: range) -> Margin:
        columns = len(xs)
        if columns == 0:
            return Margin(name, 0, 0, 0, 0, 0, False)
        first = xs[0]
        distinct: set[tuple[int, int, int]] = set()
        non_black = 0
        repeated = 0
        reference: list[tuple[int, int, int]] | None = None
        uniform = True
        for x in xs:
            column = [row[x] for row in rows]
            distinct.update(column)
            non_black += sum(1 for pixel in column if is_drawn(pixel))
            # "Repeats its neighbour" is only meaningful WITHIN the margin. Comparing the margin's
            # first column against the column outside it would count a legitimate edge column as a
            # repeat, which is how a margin full of real content could read as a black bar.
            if x > first and all(row[x] == row[x - 1] for row in rows):
                repeated += 1
            if reference is None:
                reference = column
            elif column != reference:
                uniform = False
        return Margin(name, columns, columns * height, non_black, len(distinct), repeated, uniform)

    return PictureCensus(
        width=width,
        height=height,
        left=measure("left", range(0, left_end)),
        right=measure("right", range(right_start, width)),
        drawn_columns=0 if fully_black else (max_x - min_x + 1),
        band_start=0 if fully_black else min_x,
        distinct_colours=len(colours),
        fully_black=fully_black,
    )


# ---- verdict -----------------------------------------------------------------------------------------


def verdict(width_changed: bool, narrow: PictureCensus, wide: PictureCensus) -> str:
    """Three-way, and the third state is the one that has been missing from this workspace.

    THE MEASUREMENT IS THE DRAWN BAND, NOT THE CANVAS. A wider canvas is trivially easy to produce
    and proves nothing; what a widening must do is make the SCENE occupy more columns. So a widened
    leg whose drawn band is no wider than the narrow leg's has added black and nothing else, however
    large `render_width` grew. That is the Tekken 3 card, and it is a FAILED widening.

    The per-leg margin census still reports non-black share, distinct colours and repeated columns
    with their denominators -- but it is the SECOND witness, not the first, because a margin derived
    from the non-black bounding box is black BY CONSTRUCTION and can never carry content.
    """
    if not width_changed:
        return "NOT-WIDENED"
    if narrow.fully_black or wide.fully_black:
        return "NO-PICTURE"
    if wide.drawn_columns > narrow.drawn_columns:
        return "WIDENED"
    return "FAILED"


# ---- reading a run's own log --------------------------------------------------------------------------


@dataclass
class LegReport:
    name: str
    settings: str
    exit_code: int
    wide_lines: list[tuple[int, int, int, int]] = field(default_factory=list)
    telemetry: dict[str, str] = field(default_factory=dict)
    captures: dict[int, Path] = field(default_factory=dict)
    log: str = ""

    @property
    def last_wide(self) -> tuple[int, int, int, int] | None:
        return self.wide_lines[-1] if self.wide_lines else None


def read_wide_lines(text: str) -> list[tuple[int, int, int, int]]:
    found = []
    for line in text.splitlines():
        match = WIDE_RE.search(line)
        if match:
            found.append(tuple(int(g) for g in match.groups()))  # type: ignore[arg-type]
    return found


def read_telemetry(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for name, pattern in TELEMETRY_PATTERNS.items():
        match = pattern.search(text)
        if match:
            values[name] = match.group(1)
    return values


def load_rows(path: Path) -> list[list[tuple[int, int, int]]]:
    """Decode a PNG into rows of RGB triples. The ONLY part of this tool that needs Pillow; the
    census arithmetic, the log reader and the verdict are pure, which is why `--selftest` runs in a
    bare interpreter with no image library and no disc."""
    try:
        from PIL import Image  # noqa: PLC0415
    except ImportError as error:
        raise RuntimeError(
            f"reading {path} needs Pillow, which is not installed in this interpreter. The census "
            "arithmetic itself needs nothing -- run --selftest to exercise it without Pillow"
        ) from error
    with Image.open(path) as image:
        rgb = image.convert("RGB")
        width, height = rgb.size
        flat = list(rgb.getdata())
        return [flat[y * width:(y + 1) * width] for y in range(height)]


def run_leg(binary: Path, settings: Path, label: str, seconds: float, frames: list[int],
            out_dir: Path) -> LegReport:
    out_dir.mkdir(parents=True, exist_ok=True)
    env = agent_environment(dict(os.environ), settings=settings)
    log = out_dir / f"{label}.log"
    console = out_dir / f"{label}.console"
    log.write_text("")
    env["PSXPORT_LOG_FILE"] = str(log)
    env["PSXPORT_PRESENT_SHOT_AT"] = ",".join(str(f) for f in frames)
    started = time.monotonic()
    with console.open("w") as sink:
        try:
            completed = subprocess.run([str(binary)], cwd=ROOT, env=env, stdout=sink,
                                       stderr=subprocess.STDOUT, timeout=seconds, check=False)
            code: int | str = completed.returncode
        except subprocess.TimeoutExpired:
            code = "TIMEOUT"
    elapsed = time.monotonic() - started
    text = log.read_text(errors="replace")
    shots_dir = ROOT / "scratch" / "screenshots"
    captures: dict[int, Path] = {}
    # Per-leg subdirectory, and this is a CORRECTNESS fix rather than tidiness. Both legs write the
    # product's capture to the same `scratch/screenshots/present_<frame>.png`, so copying both legs
    # into one flat directory means the SECOND leg overwrites the first and the comparison then
    # censuses the same file twice. That produced a confident "the two legs are byte-identical" on a
    # pair whose pictures plainly differ -- an instrument confirming its own input.
    leg_dir = out_dir / label
    leg_dir.mkdir(parents=True, exist_ok=True)
    for frame in frames:
        produced = shots_dir / f"present_{frame}.png"
        if produced.exists():
            target = leg_dir / f"present_{frame}.png"
            target.write_bytes(produced.read_bytes())
            produced.unlink()
            captures[frame] = target
    return LegReport(
        name=label,
        settings=str(settings),
        exit_code=code if isinstance(code, int) else -1,
        wide_lines=read_wide_lines(text),
        telemetry=read_telemetry(text),
        captures=captures,
        log=text,
    )


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()[:16]


def report_leg(leg: LegReport, frames: list[int]) -> dict[int, PictureCensus]:
    """Print one leg and RETURN its per-frame censuses, so the verdict can compare legs."""
    print(f"\n[leg {leg.name}] settings={leg.settings} exit={leg.exit_code}")
    if not leg.wide_lines:
        print("  NO [wide] line at all: the product never announced its picture geometry. A leg with "
              "no announcement cannot be called widened OR not-widened; it is UNMEASURED.")
    else:
        print(f"  [wide] lines emitted: {len(leg.wide_lines)} (the LAST is the one to quote -- "
              "picture_announce prints on CHANGE)")
        for aspect, engine, native, render in leg.wide_lines:
            print(f"    aspect={aspect} wide_engine={engine} native_width={native} render_width={render}")
    print(f"  guest telemetry: {leg.telemetry}")
    censuses: dict[int, PictureCensus] = {}
    for frame in frames:
        path = leg.captures.get(frame)
        if path is None:
            print(f"  frame {frame}: NO CAPTURE WRITTEN. Not 'black' -- absent, and a frame this "
                  "tool did not see cannot support a verdict.")
            continue
        try:
            census = census_from_rows(load_rows(path))
        except RuntimeError as error:
            print(f"  frame {frame}: {error}")
            continue
        censuses[frame] = census
        aspect = census.drawn_columns / census.height if census.height else 0.0
        print(f"  frame {frame}: canvas {census.width}x{census.height}, DRAWN BAND {census.drawn_columns} "
              f"col(s) (aspect {aspect:.3f}), {census.distinct_colours} distinct colour(s) in the whole "
              f"frame, sha={digest(path)}")
        print(f"    {census.left.line()}")
        print(f"    {census.right.line()}")
    return censuses


def compare_telemetry(narrow: LegReport, wide: LegReport) -> bool:
    """Guest-execution telemetry must be IDENTICAL between the legs. Widescreen is a presentation
    change; if the simulation moved, the widening reached into guest state and every claim about it
    is void."""
    shared = sorted(set(narrow.telemetry) & set(wide.telemetry))
    if not shared:
        print("\n[telemetry] NO shared measurement between the legs -- the two runs cannot be "
              "compared, which is NOT evidence that they agreed.")
        return False
    same = True
    for key in shared:
        a, b = narrow.telemetry[key], wide.telemetry[key]
        agree = a == b
        same = same and agree
        print(f"\n[telemetry] {key}: 4:3={a} 16:9={b} -> {'identical' if agree else 'DIFFERENT'}")
    return same


# ---- the self-check: negative first, and no disc, no game, no Pillow ----------------------------------


def _solid(width: int, height: int, colour) -> list[list[tuple[int, int, int]]]:
    return [[colour for _ in range(width)] for _ in range(height)]


def _gradient(width: int, height: int) -> list[list[tuple[int, int, int]]]:
    return [[(200, 40 + (x % 100), 90, ) [:3] for x in range(width)] for _ in range(height)]


def selftest(out=print) -> int:
    checks = 0
    failures = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks, failures
        checks += 1
        if not condition:
            failures += 1
            out(f"  FAIL: {message}")

    narrow_full = census_from_rows(_gradient(64, 32))
    check(narrow_full.left.columns == 0 and narrow_full.right.columns == 0,
          "a full-bleed picture reports zero margin columns")
    check(narrow_full.drawn_columns == 64, "a full-bleed picture's drawn band is the whole width")

    # (1) THE NEGATIVE THAT MATTERS: a wider canvas holding the SAME picture. The drawn band did not
    #     grow, so nothing was re-projected -- this is the shape a "widescreen" claim must fail on.
    narrow_pixels = _gradient(64, 32)
    pillarboxed = _solid(96, 32, (0, 0, 0))
    for y in range(32):
        for x in range(16, 80):
            pillarboxed[y][x] = narrow_pixels[y][x - 16]
    wide_pillar = census_from_rows(pillarboxed)
    check(wide_pillar.left.columns == 16, f"left margin is 16 columns, got {wide_pillar.left.columns}")
    check(wide_pillar.right.columns == 16, f"right margin is 16 columns, got {wide_pillar.right.columns}")
    check(wide_pillar.left.all_black and wide_pillar.right.all_black, "a pillarbox margin is black")
    check(wide_pillar.left.non_black_share == 0.0, "a black margin's non-black share is exactly zero")
    check(wide_pillar.margins_are_blank, "a pillarboxed picture reports blank margins")
    check(wide_pillar.uniform_margins, "both pillarbox margins are one flat colour")
    check(wide_pillar.drawn_columns == 64, "the drawn band is the 4:3 picture, not the wider canvas")
    check(verdict(True, narrow_full, wide_pillar) == "FAILED",
          f"a pillarboxed picture must be FAILED, got {verdict(True, narrow_full, wide_pillar)}")

    # (2) A genuinely re-projected frame: the scene occupies MORE columns than the narrow leg. This
    #     is the answer the tool must be able to give, and it is only trustworthy because (1) is
    #     told apart from it.
    wide_real = census_from_rows(_gradient(96, 32))
    check(wide_real.drawn_columns == 96, "a re-projected frame's drawn band is the whole canvas")
    check(wide_real.left.columns == 0, "a re-projected frame has no margin to be black")
    check(verdict(True, narrow_full, wide_real) == "WIDENED",
          f"a re-projected frame is WIDENED, got {verdict(True, narrow_full, wide_real)}")

    # (3) THE KNOWN LIMIT, PINNED AS A POSITIVE CASE. A STRETCH of the 4:3 picture into the wider
    #     canvas also grows the drawn band, so the band test alone cannot separate a stretch from a
    #     re-projection. The tool says so rather than implying it can, and this is why the picture
    #     pair stays a human judgement: the tool refuses the black-margin case, it does not certify
    #     the non-black one.
    wide_stretch = census_from_rows(_gradient(96, 32))
    check(verdict(True, narrow_full, wide_stretch) == "WIDENED",
          "a stretched frame grows the drawn band and is not caught by the band test alone")
    check(wide_stretch.drawn_columns > narrow_full.drawn_columns,
          "the drawn band cannot by itself tell a stretch from a re-projection")

    # (4) NOT-WIDENED is reachable whatever the margins say: the width gate is separate.
    check(verdict(False, narrow_full, wide_pillar) == "NOT-WIDENED",
          "a leg whose width did not grow is NOT-WIDENED whatever its margins say")

    # (5) An entirely black frame is its own state, so a run that presented nothing cannot be read as
    #     a picture, and a FAILED widening stays distinguishable from no picture at all.
    black = census_from_rows(_solid(64, 32, (0, 0, 0)))
    check(black.fully_black, "an all-black frame is reported fully black")
    check(black.drawn_columns == 0, "an all-black frame has a zero-width drawn band")
    check(verdict(True, narrow_full, black) == "NO-PICTURE",
          f"an all-black wide leg is NO-PICTURE, got {verdict(True, narrow_full, black)}")
    check(verdict(True, black, black) == "NO-PICTURE",
          "an all-black narrow leg is NO-PICTURE too, not a comparison")

    # (6) A margin taken from the non-black bounding box is black BY CONSTRUCTION, so a "partly drawn
    #     margin" is not a state this instrument can be in -- the first version of this case tried to
    #     build one and silently measured a 2-column margin instead of the 20 it asserted, which is
    #     the same class of error as a scan of the wrong shape returning a confident zero.
    #     What IS measurable, and is asserted here, is that the margin reports a real share against
    #     a real denominator, and that the picture's position in its canvas is a NUMBER.
    check(wide_pillar.left.non_black == 0 and wide_pillar.left.pixels == 16 * 32,
          f"the left margin is 0/{16 * 32} non-black, got {wide_pillar.left.non_black}/{wide_pillar.left.pixels}")
    check(wide_pillar.left.non_black_share == 0.0, "a pillarbox margin's share is exactly 0.0")
    check(wide_pillar.centring_offset == 16,
          f"a re-centred picture sits 16 columns in, got {wide_pillar.centring_offset}")
    check(narrow_full.centring_offset == 0, "a full-bleed picture's centring offset is 0")
    check(wide_real.centring_offset == 0, "a picture that grew to its new edge has offset 0")

    # (7) A ragged or empty image is REFUSED rather than censused on a partial read. A census over a
    #     short read is the exact shape of "0 of N" becoming "zero".
    for name, rows in (("ragged", [_solid(8, 4, (1, 2, 3))[0], _solid(4, 4, (1, 2, 3))[0]]),
                       ("empty", [])):
        try:
            census_from_rows(rows)
            check(False, f"a {name} image must be refused")
        except ValueError:
            check(True, f"a {name} image is refused")

    # (8) The `[wide]` reader counts every line and keeps the LAST, because picture_announce prints
    #     on CHANGE and quoting the first reads "not widened" on a leg that is.
    text = ("[wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=512\n"
            "[wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=684\n")
    lines = read_wide_lines(text)
    check(len(lines) == 2, f"two [wide] lines are both read, got {len(lines)}")
    check(lines[-1][3] == 684, f"the LAST line is the one quoted, got render_width={lines[-1][3]}")
    check(read_wide_lines("no announcement here") == [],
          "a log with no [wide] line yields none, not a default")

    # (10) THE BUG THIS TOOL SHIPPED WITH, pinned so it cannot come back quietly. The leg identities
    #     were written out in three places and the dictionary key was "16x9" while every lookup said
    #     "16:9". The wide leg was therefore never reported, the tool printed UNMEASURED, and the
    #     same run printed a sorted key list containing '16x9' -- which reads as though the leg WAS
    #     there. A missing leg must be impossible to confuse with a present one, so the two facts the
    #     verdict depends on are asserted directly against the single declaration they come from.
    check(len(LEG_NAMES) == len(set(LEG_NAMES)), "the two leg names are distinct")
    check(set(LEG_NAMES) == set(LEG_SETTINGS),
          f"the names and the settings table name the same legs: {set(LEG_NAMES) ^ set(LEG_SETTINGS)}")
    for name, settings in LEG_SETTINGS.items():
        check(settings.is_file(), f"leg {name} names a settings file that exists ({settings.name})")
    # The typo itself, as a check on THIS file's own source: a leg key that does not match any
    # declared leg is the defect, and a grep of the shipped file is the only instrument that sees it.
    # CODE lines only: the comment that documents this very bug necessarily contains the wrong
    # spelling, and a check that trips on its own explanation is a check nobody will keep.
    code = [ln for ln in Path(__file__).read_text().splitlines() if not ln.lstrip().startswith("#")]
    # Assembled rather than written out, so this probe's own source does not contain the spelling
    # it is looking for -- otherwise the check trips on itself, which is the third variant of the
    # same mistake in as many edits.
    typos = {"16" + "x9", "4" + "3x3"}
    stray = {m for m in typos if any(f'"{m}"' in ln for ln in code)}
    check(not stray, f"no mistyped leg key survives in this file's CODE: {sorted(stray)}")
    # And the lookups the verdict actually performs must resolve against a populated report.
    both = {name: LegReport(name, str(LEG_SETTINGS[name]), 0) for name in LEG_NAMES}
    check(all(both.get(name) is not None for name in LEG_NAMES),
          "every declared leg resolves in a report built from the same declaration")

    # (9) Telemetry disagreement must be detectable, since a leg whose simulation moved voids every
    #     widescreen claim no matter what the picture shows.
    a = LegReport("4:3", "x", 0, telemetry={"fields": "29034", "publications": "176"})
    b = LegReport("16:9", "y", 0, telemetry={"fields": "29034", "publications": "176"})
    c = LegReport("16:9", "y", 0, telemetry={"fields": "29033", "publications": "176"})
    check(compare_telemetry(a, b), "identical telemetry compares equal")
    check(not compare_telemetry(a, c), "a one-field difference is detected")
    d = LegReport("16:9", "y", 0, telemetry={"other": "1"})
    check(not compare_telemetry(a, d), "disjoint telemetry cannot be compared and is not agreement")

    out(f"  {checks - failures}/{checks} checks passed")
    return 0 if failures == 0 else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", default=str(ROOT / "build" / "agent-clang" / "ctr_port"))
    parser.add_argument("--seconds", type=float, default=200.0)
    parser.add_argument("--frames", default="50,100")
    parser.add_argument("--out", default=str(ROOT / "scratch" / "widescreen"))
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    frames = [int(f) for f in args.frames.split(",") if f]
    out_dir = Path(args.out).resolve()
    binary = Path(args.binary).resolve()
    reports = {name: run_leg(binary, settings, name, args.seconds, frames, out_dir)
               for name, settings in LEG_SETTINGS.items()}

    # Reported one leg at a time, and explicitly, rather than through a comprehension. A missing leg
    # is a refusal with a name, and `censuses["16x9"]` raising KeyError is neither.
    censuses: dict[str, dict[int, PictureCensus]] = {}
    for name in LEG_SETTINGS:
        leg = reports.get(name)
        if leg is None:
            print(f"\n[leg {name}] ABSENT from this run's reports; it is not measured and no verdict "
                  "below may speak for it.")
            continue
        censuses[name] = report_leg(leg, frames)

    narrow, wide_key = LEG_NAMES
    narrow_last = reports[narrow].last_wide if reports.get(narrow) else None
    wide_last = reports[wide_key].last_wide if reports.get(wide_key) else None
    grew = False
    print()
    if not reports.get(narrow) or not reports.get(wide_key):
        # A missing leg is its own refusal, and it names what IS there rather than raising a KeyError
        # on a dictionary key a reader has never seen.
        print(f"[verdict] legs present: {sorted(reports)}; a missing leg makes this run UNMEASURED, "
              "not a pass and not a failure.")
    if narrow_last and wide_last:
        grew = wide_last[3] > wide_last[2]
        print(f"[verdict] {narrow}  native_width={narrow_last[2]} render_width={narrow_last[3]}")
        print(f"[verdict] {wide_key} native_width={wide_last[2]} render_width={wide_last[3]}")
        print(f"[verdict] the requested width {'GREW' if grew else 'DID NOT GROW'}")
    else:
        print("[verdict] a leg never announced its picture geometry, so this run cannot answer "
              "whether the width grew. Reported as UNMEASURED, not as a pass.")

    for frame in frames:
        narrow = censuses.get("4x3", {}).get(frame)
        wide = censuses.get("16:9", {}).get(frame)
        if narrow_census is None or wide_census is None:
            print(f"[verdict] frame {frame}: UNMEASURED (a capture is missing on at least one leg)")
            continue
        call = verdict(grew, narrow_census, wide_census)
        print(f"[verdict] frame {frame}: drawn band {narrow_census.drawn_columns} -> "
              f"{wide_census.drawn_columns} column(s) on a canvas {narrow_census.width} -> "
              f"{wide_census.width} wide -> {call}")
        if call == "FAILED":
            print(f"[verdict] frame {frame}: the canvas grew but the drawn band did not, so the extra "
                  f"width is black margin. This is a FAILED widening and is reported as one.")

    identical = compare_telemetry(reports["4x3"], reports["16:9"])
    print(f"[verdict] guest-execution telemetry across the two legs: "
          f"{'IDENTICAL' if identical else 'NOT COMPARABLE or DIFFERENT'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
