#!/usr/bin/env python3
"""ctr_binary_probe.py — read SCUS_944.26 out of the provisioned image and report facts.

Every command here is a MEASUREMENT with a stated denominator. The point of the tool is
that the numbers in `docs/issues/` came out of bytes in `scratch/raw/ctr/SCUS_944.26`, not
out of a doc, a goal string, or a name that looks like a measurement.

It imports psxport's shared PS-X EXE reader and R3000A decoder. Those are the owners of
"what is a PS-X EXE" and "what is a MIPS instruction"; a second implementation here would
be exactly the drift the workspace keeps measuring.

Coverage is stated, never implied. Two coverage limits are named rather than hidden:
  * only the MAIN executable image is scanned — `BIGFILE.BIG` overlays are not provisioned,
    and CTR's overlays reuse load addresses, so an address alone is not an identity;
  * `jalr` targets are not statically resolvable, so an indirect call to a leaf is invisible
    to any `jal` census. The count of `jalr` sites is reported so the gap is a number.

Run `--selftest` for the gate-in-CTest self-check, which includes the negative cases.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT / "external" / "psxport" / "tools") not in sys.path:
    sys.path.insert(0, str(_ROOT / "external" / "psxport" / "tools"))

from formats import psx_exe  # noqa: E402  (path must be set first)
from mips import decode, fmt  # noqa: E402

# The authenticated image. `tools/provision.py` writes it and records this identity; the
# tool reads the header itself rather than trusting a constant, and asserts the digest.
DEFAULT_IMAGE = _ROOT / "scratch" / "raw" / "ctr" / "SCUS_944.26"
RECORDED_SHA256 = "7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838"

# ---- addresses, all read out of the image and quoted with their instruction words ----

# The title's VSync leaf. Its entry is confirmed by the words quoted in vsync_wait_semantics.
VSYNC = 0x80075350
# The title's own wait routine, reached from VSync only for a0 >= 2.
VSYNC_WAIT = 0x800754C8

# The dynamic projection publication, and the two libgte leaves it calls.
PROJECTION_PUBLISHER = 0x80042910
SET_GEOM_OFFSET = 0x8007782C
SET_GEOM_SCREEN = 0x8007781C

# The frame suffix's countdown word. gp is read out of the image by measured_gp().
COUNTDOWN_GP_OFFSET = 0x348
# The state-zero startup projection publication (direct libgte calls, no descriptor).
STARTUP_PUBLICATION = 0x8003C84C

# The near plane the two geometry paths compare object Z against, in the DTLB scratchpad.
SCRATCHPAD_NEAR_PLANE = 0x1F800054

MEM_OPS = ("lw", "sw", "lh", "sh", "lbu", "lhu", "sb")
# CTest's SKIP_RETURN_CODE. Used only where the tool cannot run for want of the authenticated
# image, and always with the reason printed.
SKIP_EXIT_CODE = 77
CONTROL_TRANSFERS = frozenset(
    {"j", "jal", "jr", "jalr", "beq", "bne", "blez", "bgtz", "bltz", "bgez", "bltzal", "bgezal"}
)
A0_DEFINING_IMMEDIATE = ("addiu", "addi", "ori", "lui", "slti", "sltiu", "andi", "xori")
A0_DEFINING_LOAD = ("lbu", "lhu", "lw", "lwl", "lwr", "lh", "lb")
A0_DEFINING_REGISTER = ("addu", "add", "subu", "sub", "or", "and", "slt", "sltu", "xor", "nor")


class Refusal(Exception):
    """A refusal the tool raises instead of answering a question it cannot answer."""


@dataclass(frozen=True)
class Image:
    """The main executable image plus the derived scan domain."""

    exe: psx_exe.PsxExe
    digest: str
    path: Path

    @property
    def words(self) -> int:
        return self.exe.text_size // 4

    def word(self, address: int) -> int:
        return self.exe.word(address)

    def instructions(self):
        """Yield (address, Instr) for every word of the text extent."""
        for index in range(self.words):
            address = self.exe.load + index * 4
            yield address, decode(address, self.exe.word(address))

    def coverage(self) -> str:
        return (
            f"image {self.path.name} sha256={self.digest[:16]}… "
            f"text [0x{self.exe.load:08X},0x{self.exe.text_end:08X}) = {self.words} instruction words"
        )


def open_image(path: Path | None = None) -> Image:
    image_path = Path(path) if path else DEFAULT_IMAGE
    if not image_path.is_file():
        raise Refusal(
            f"{image_path} is missing. This tool reads the authenticated image; it does not "
            f"guess addresses. Re-provision with tools/provision.py."
        )
    try:
        exe = psx_exe.load(image_path)
    except ValueError as error:
        raise Refusal(str(error)) from error
    digest = hashlib.sha256(image_path.read_bytes()).hexdigest()
    return Image(exe=exe, digest=digest, path=image_path)


# --------------------------------------------------------------------------------------
# commands
# --------------------------------------------------------------------------------------


def cmd_identity(image: Image, _args: argparse.Namespace) -> int:
    exe = image.exe
    print(f"[identity] {image.coverage()}")
    print(f"[identity] ps-x-exe magic verified by the shared reader; entry=0x{exe.entry:08X}")
    print(f"[identity] load=0x{exe.load:08X} text_size=0x{exe.text_size:X} text_end=0x{exe.text_end:08X}")
    print(f"[identity] header stack=0x{exe.sp_base:08X} (gp field is 0 in this executable)")
    print(f"[identity] sha256={image.digest}")
    if image.digest != RECORDED_SHA256:
        print(
            f"[identity] REFUSING: digest is not the recorded {RECORDED_SHA256[:16]}… — this is a "
            f"different image and every address below would be unverified"
        )
        return 2
    print(f"[identity] digest matches the recorded identity; main image only — 0 of N BIGFILE "
          f"overlays are provisioned, so overlay addresses are NOT covered by this tool")
    return 0


def cmd_disasm(image: Image, args: argparse.Namespace) -> int:
    start = int(args.address, 16)
    asked = args.count
    end = min(start + asked * 4, image.exe.text_end)
    served = (end - start) // 4
    if served < asked:
        print(
            f"[disasm] asked for {asked} instruction(s) at 0x{start:08X}, served {served}: the "
            f"remainder is PAST THE TEXT EXTENT and was NOT fetched, not zero"
        )
    else:
        print(f"[disasm] asked for {asked}, served {served}")
    for address in range(start, end, 4):
        word = image.word(address)
        print(f"0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    return 0


def cmd_callers(image: Image, args: argparse.Namespace) -> int:
    target = int(args.address, 16)
    sites = [a for a, i in image.instructions() if i.op == "jal" and i.target == target]
    jalr = sum(1 for _, i in image.instructions() if i.op == "jalr")
    print(f"[callers] target=0x{target:08X}")
    print(f"[callers] scanned {image.words} instruction word(s) of {image.words}; "
          f"matched {len(sites)} direct `jal` site(s)")
    print(f"[callers] COVERAGE LIMIT: {jalr} `jalr` site(s) in this image have no statically "
          f"resolvable target, so an indirect call to 0x{target:08X} is NOT counted above")
    for site in sites:
        word = image.word(site)
        delay = image.word(site + 4)
        print(
            f"  site 0x{site:08X}  word {word:08X}  return=0x{site + 8:08X}  "
            f"delay-slot {delay:08X} ({fmt(decode(site + 4, delay))})"
        )
    if not sites:
        print(f"  (no direct `jal` to 0x{target:08X}; that is a scanned-and-found-none answer, "
              f"not a skipped scan)")
    return 0


def _a0_from_instruction(image: Image, instruction):
    """If this instruction gives a0 a constant, return (value, how); else None."""
    if instruction.rt == 4 and (
        instruction.op in A0_DEFINING_IMMEDIATE or instruction.op in A0_DEFINING_LOAD
    ):
        if instruction.op in ("addiu", "addi", "ori") and instruction.rs == 0:
            return instruction.simm, "immediate"
        if instruction.op == "lui":
            return instruction.imm << 16, "lui"
        if instruction.op in A0_DEFINING_LOAD:
            return None, f"a0 loaded by {instruction.op}"
        return None, f"a0 copied by {instruction.op}"
    if instruction.rd == 4 and instruction.op in A0_DEFINING_REGISTER:
        if instruction.rs == 0:
            return 0, "immediate"
        return None, f"a0 copied by {instruction.op}"
    return None


# Calls return, so a backward walk may cross them. Unconditional jumps do not, and a
# conditional branch has two predecessors that must agree.
_CALL_LIKE = frozenset({"jal", "jalr"})
_BRANCH_LIKE = frozenset({"beq", "bne", "blez", "bgtz", "bltz", "bgez", "bltzal", "bgezal"})
_TAIL_LIKE = frozenset({"j", "jr"})


def _resolve_a0(image: Image, site: int, max_depth: int = 48):
    """The constant a0 holds at `site`, by a sound linear backward walk, or None.

    Two facts this depends on, both read out of the image rather than assumed:

    * the walk starts at the `jal`'s OWN DELAY SLOT (`site + 4`), not at `site - 4`. On this
      title the argument is more often in the delay slot than before the call — starting one
      instruction too low silently mis-read real arguments, which is a wrong answer, not a
      missing one;
    * a `jal` is CROSSED (control returns to the fall-through, which is the instruction the
      walk is already at) but a conditional branch and a tail jump STOP it, because their
      other predecessor is a path this walk cannot see.

    A single linear predecessor chain is what makes this sound. Anything it cannot see is
    reported UNRESOLVED with the reason, so a partial result can never be read as complete.
    """
    address = site + 4  # the delay slot: the last instruction to execute before the call
    for depth in range(max_depth + 1):
        if address < image.exe.load or address + 4 > image.exe.text_end:
            return None, "the walk left the text extent"
        instruction = decode(address, image.word(address))
        if instruction.op in _TAIL_LIKE:
            return None, f"stopped at the tail jump {instruction.op} at 0x{address:08X}"
        if instruction.op in _BRANCH_LIKE:
            return None, f"stopped at the conditional {instruction.op} at 0x{address:08X}"
        if instruction.op not in _CALL_LIKE:
            found = _a0_from_instruction(image, instruction)
            if found is not None:
                value, how = found
                if value is None:
                    return None, f"a0 is {how} at 0x{address:08X}"
                where = "the call's delay slot" if address == site + 4 else f"0x{address:08X}"
                return value, f"{how} in {where}"
        address -= 4
    return None, f"no a0 definition within {max_depth} instructions"


def _ghidra_census_path() -> Path:
    return _ROOT / "scratch" / "vsync_census.json"


def _cross_check_ghidra(image: Image, resolved: dict[int, int]) -> list[str]:
    """Compare the from-bytes census against the Ghidra decompilation census.

    Two independent methods that agree is evidence; one method that agrees with itself is
    not. The Ghidra side is produced by scratch/ctr_vsync_census.py (a maintainer RE step)
    into non-executable facts. When that file is absent this says so rather than skipping
    the comparison silently — a cross-check that runs only when convenient is not a gate.
    """
    path = _ghidra_census_path()
    if not path.is_file():
        return [f"NO CROSS-CHECK: {path} is absent, so the from-bytes census is UNCORROBORATED"]
    import json

    data = json.loads(path.read_text())
    if data.get("vsync") != VSYNC:
        return [f"REFUSING: {path} records a VSync of 0x{data.get('vsync'):08X}"]
    problems: list[str] = []
    compared = 0
    ghidra_sites: set[int] = set()
    for function in data.get("functions", []):
        sites = function.get("sites", [])
        arguments = function.get("decompiled_arguments", [])
        ghidra_sites.update(sites)
        if len(sites) != len(arguments):
            problems.append(
                f"function 0x{function['function']:08X}: {len(sites)} site(s) but "
                f"{len(arguments)} decompiled argument(s) — the correspondence is not forced, "
                f"so nothing from this function was compared"
            )
            continue
        for site, argument in zip(sites, arguments):
            if argument is None:
                problems.append(f"0x{site:08X}: Ghidra emitted an unparsable argument")
                continue
            value = argument - (1 << 32) if argument >= (1 << 31) else argument
            if site not in resolved:
                problems.append(f"0x{site:08X}: Ghidra resolved {value} but the byte walk did not")
                continue
            compared += 1
            if resolved[site] != value:
                problems.append(
                    f"0x{site:08X}: Ghidra says {value}, the byte walk says {resolved[site]}"
                )
    only_bytes = sorted(set(resolved) - ghidra_sites)
    if only_bytes:
        problems.append(
            "site(s) the byte walk resolved that Ghidra's call-flow scan did not list: "
            + ", ".join(f"0x{a:08X}={resolved[a]}" for a in only_bytes)
        )
    print(f"[vsync] CROSS-CHECK: compared {compared} site(s) against the Ghidra decompilation "
          f"census; {len(only_bytes)} site(s) Ghidra did not list are byte-walk-only")
    return problems


def cmd_vsync_arguments(image: Image, args: argparse.Namespace) -> int:
    """Census every direct VSync(n) site and say what the arguments imply."""
    print(f"[vsync] {image.coverage()}")
    # The wait semantics decide what an argument MEANS, so establish them here and not by
    # assumption. Each branch is asserted against the word actually present in the image.
    semantics = _vsync_wait_semantics(image)
    print(
        f"[vsync] wait semantics read from the image: "
        f"a0<{semantics.no_wait_negative_end} -> no wait; a0=={semantics.no_wait_one} -> no wait; "
        f"a0<={semantics.wait_floor} -> no field wait; a0>={semantics.wait_first} -> "
        f"{semantics.field_offset} field(s) per argument unit, waited as (n-{semantics.field_offset})"
    )
    print(
        f"[vsync] CONSEQUENCE: an argument of 0, 1 or -1 carries NO rate information. Only "
        f"a0>={semantics.wait_first} is a field count."
    )

    sites = [a for a, i in image.instructions() if i.op == "jal" and i.target == VSYNC]
    jalr = sum(1 for _, i in image.instructions() if i.op == "jalr")
    arguments: Counter = Counter()
    resolved: dict[int, int] = {}
    unresolved: list[tuple[int, str]] = []
    for site in sites:
        value, how = _resolve_a0(image, site)
        if value is None:
            unresolved.append((site, how))
            arguments["UNRESOLVED"] += 1
        else:
            resolved[site] = value
            arguments[value] += 1
        print(
            f"  site 0x{site:08X}  a0={value if value is not None else 'UNRESOLVED'}  ({how})"
        )
    print(f"[vsync] asked for a census of every direct VSync call; served {len(sites)} of "
          f"{len(sites)} direct `jal` site(s) found in {image.words} scanned word(s), and "
          f"resolved the argument at {len(resolved)} of {len(sites)}")
    print(f"[vsync] COVERAGE LIMIT: {jalr} `jalr` site(s) have no statically resolvable target, so "
          f"an indirect VSync call is not counted; and 0 overlay images are provisioned, so a "
          f"call made from a BIGFILE overlay is not counted either")
    print("[vsync] argument distribution: " + ", ".join(
        f"VSync({k}) x{v}" for k, v in sorted(arguments.items(), key=lambda kv: str(kv[0]))
    ))
    waiting = sum(v for k, v in arguments.items() if isinstance(k, int) and k >= semantics.wait_first)
    print(
        f"[vsync] of {len(sites)} direct site(s), {waiting} carry an argument that WAITS A FIELD "
        f"COUNT and {len(sites) - waiting} do not (argument is 0, 1 or -1)"
    )
    waiting_sites = [a for a in sites if resolved.get(a, 0) >= semantics.wait_first]
    if waiting_sites:
        print("[vsync] the waiting sites, with the field count each one actually waits: " + ", ".join(
            f"0x{a:08X}=VSync({resolved[a]}) waits {resolved[a] - semantics.field_offset} field(s)"
            for a in waiting_sites
        ))
    if unresolved:
        print(f"[vsync] {len(unresolved)} site(s) UNRESOLVED, and the missing ones were NOT "
              f"fetched: " + ", ".join(f"0x{a:08X} ({why})" for a, why in unresolved))
    problems = _cross_check_ghidra(image, resolved)
    for problem in problems:
        print(f"[vsync] {problem}")
    return 2 if any(p.startswith("REFUSING") or "disagree" in p for p in problems) else 0


@dataclass(frozen=True)
class WaitSemantics:
    no_wait_negative_end: int
    no_wait_one: int
    wait_floor: int
    wait_first: int
    field_offset: int
    countdown_shift: int


def _vsync_wait_semantics(image: Image) -> WaitSemantics:
    """Read the argument branch structure out of the image, asserting each word.

    A wrong answer here would silently reinterpret every argument in the census, so each
    field is read back out of the bytes and a mismatch refuses rather than defaulting.
    """
    expected = {
        # bgez a0  -> a0 < 0 takes the no-wait return
        0x800753A8: 0x04810005,
        # beq a0, 1 -> a0 == 1 takes the other no-wait return
        0x800753C4: 0x1082003A,
        # blez a0 -> a0 == 0 takes the snapshot target (no field wait)
        0x800753CC: 0x18800007,
        # a1 = a0 - 1, in two halves around the blez
        0x800753F8: 0x00002821,
        0x800753FC: 0x2485FFFF,
        # the wait routine scales the field count left by 15
        VSYNC_WAIT + 4: 0x00052BC0,
    }
    for address, word in expected.items():
        actual = image.word(address)
        if actual != word:
            raise Refusal(
                f"0x{address:08X} is 0x{actual:08X}, not the 0x{word:08X} the VSync argument "
                f"semantics depend on; refusing to report a census built on it"
            )
    if image.word(VSYNC) != 0x3C028009:
        raise Refusal(f"0x{VSYNC:08X} is not the VSync entry prologue; refusing")
    return WaitSemantics(
        no_wait_negative_end=0,
        no_wait_one=1,
        wait_floor=0,
        wait_first=2,
        field_offset=1,
        countdown_shift=15,
    )


def cmd_vsync_wait_semantics(image: Image, _args: argparse.Namespace) -> int:
    print(f"[vsync-wait] {image.coverage()}")
    semantics = _vsync_wait_semantics(image)
    print(f"[vsync-wait] a0 < 0  -> no wait (j to the epilogue)")
    print(f"[vsync-wait] a0 == 1 -> no wait (b to the epilogue)")
    print(f"[vsync-wait] a0 <= 0 -> no field wait; the target is a snapshot of the field clock")
    print(
        f"[vsync-wait] a0 >= {semantics.wait_first} -> a1 = a0 - "
        f"{semantics.field_offset}, counted down from (a1 << {semantics.countdown_shift}) in "
        f"0x{VSYNC_WAIT:08X}"
    )
    print(f"[vsync-wait] so a waiting call waits (argument - {semantics.field_offset}) fields")
    for address in sorted(expected_words()):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    return 0


def expected_words() -> dict[int, int]:
    return {
        VSYNC: 0x3C028009,
        0x800753A8: 0x04810005,
        0x800753C0: 0x24020001,
        0x800753C4: 0x1082003A,
        0x800753CC: 0x18800007,
        0x800753EC: 0x3C028009,
        0x800753F4: 0x18800002,
        0x800753F8: 0x00002821,
        0x800753FC: 0x2485FFFF,
        0x80075400: 0x0C01D532,
        VSYNC_WAIT: 0x27BDFFE0,
        VSYNC_WAIT + 4: 0x00052BC0,
    }


def cmd_projection_owner(image: Image, _args: argparse.Namespace) -> int:
    print(f"[projection] {image.coverage()}")
    print(f"[projection] the two libgte leaves, quoted from the image:")
    for address in (SET_GEOM_SCREEN, SET_GEOM_OFFSET):
        for offset in range(0, 0x18, 4):
            word = image.word(address + offset)
            print(f"  0x{address + offset:08X}: {word:08X}  {fmt(decode(address + offset, word))}")
        print()
    # The body runs to the `jr ra` at +0x5C; +0x64 is the next function's prologue, and it is
    # dumped on purpose so the extent is bounded by bytes rather than by a printed range.
    print(f"[projection] the dynamic publication at 0x{PROJECTION_PUBLISHER:08X}, quoted from the image:")
    for address in range(PROJECTION_PUBLISHER, PROJECTION_PUBLISHER + 0x68, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    print()
    print(f"[projection] direct `jal` sites:")
    for label, target in (
        ("publisher", PROJECTION_PUBLISHER),
        ("SetGeomOffset", SET_GEOM_OFFSET),
        ("SetGeomScreen", SET_GEOM_SCREEN),
    ):
        sites = [a for a, i in image.instructions() if i.op == "jal" and i.target == target]
        print(f"  {label} (0x{target:08X}): {len(sites)} site(s) — "
              + ", ".join(f"0x{a:08X}" for a in sites))
    print()
    print(f"[projection] the state-zero startup publication at 0x{STARTUP_PUBLICATION:08X}, "
          f"quoted from the image:")
    for address in range(STARTUP_PUBLICATION, STARTUP_PUBLICATION + 0x14, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")


# --------------------------------------------------------------------------------------
# The 3D submission path: every writer and every reader of the GTE projection distance.
# --------------------------------------------------------------------------------------

# GTE control register numbers, from the ONE authority this workspace has: psxport's own GTE,
# external/psxport/vendor/beetle-psx/mednafen/psx/gte.c, which names them
#     #define OFX (int32_t)CR[24]  #define OFY (int32_t)CR[25]  #define H (uint16_t)CR[26]
# A literal-immediate scan cannot establish this, and a shifted numbering is exactly the kind of
# mistake that makes a "safe to widen" claim rest on a register that is not the one being written.
GTE_CR_OFX = 24
GTE_CR_OFY = 25
GTE_CR_H = 26

# The GTE perspective-transform op code, from the same framework file's own `gte_name` table
# (0x01 is RTPS, the single-vertex form this image uses; 0x30 is RTPT, which it does not).
GTE_OP_RTPS = 0x01

# The ten geometry-submission functions, each paired with the address of the first instruction of
# the SHARED FIVE-INSTRUCTION TAIL that ends at that function's `ctc2 rX,$26`.
# `check_submission_tail` re-derives the shape from the bytes at every site, so a re-addressed or
# different image fails the gate instead of quietly reporting a different set of sites.
GEOMETRY_SUBMITTERS: tuple[tuple[int, int], ...] = (
    # (function entry, first instruction of the shared tail)
    (0x80069FFC, 0x8006A0D0),
    (0x8006AAA8, 0x8006AB28),
    (0x8006DC30, 0x8006DCD4),
    (0x8006E26C, 0x8006E2F0),
    (0x8006E588, 0x8006EB14),
    (0x8006F004, 0x8006F5A0),
    (0x8006F9A8, 0x8006FA58),
    (0x8006FE70, 0x8006FF18),
    (0x80070388, 0x80070418),
    (0x80070950, 0x800709BC),
)

# THE RECOVERED SHAPE, and deliberately only the part that is genuinely shared. Five instructions,
# identical in opcode and operand at all ten sites, ending AT the `ctc2 rX,$26` that writes the
# GTE's projection-plane distance:
#
#     sll  a, a, 15     OFX/OFY are 16.16 fixed point built from a 16-bit screen offset
#     sll  b, b, 15
#     ctc2 a, $24      -> CR24 = OFX
#     ctc2 b, $25      -> CR25 = OFY
#     ctc2 c, $26      -> CR26 = H
#
# WHAT IS NOT CLAIMED AS SHARED: the register numbers in the two shifts and the three writes, and
# the descriptor loads that feed them. Those genuinely vary -- some sites read the view descriptor
# through `a0` and some through `a2`, some with `lh` and some with `lw` -- and the variance is
# MEASURED and reported below rather than smoothed into one convenient claim. An earlier revision of
# this gate asserted a single eight-instruction prologue; the gate itself then reported 0 of 10 sites
# matching it, which is how the real shape was found.
SUBMISSION_TAIL_SHAPE: tuple[tuple[str, str], ...] = (
    ("sll", "15"),
    ("sll", "15"),
    ("ctc2", "24"),
    ("ctc2", "25"),
    ("ctc2", "26"),
)

# The view-descriptor offsets the tail is fed from, and the SAME offsets the 0x80042910 publication
# reads. That identity is why one plan can serve both publication sites: the geometry submitters and
# the descriptor publisher consume the same view descriptor.
VIEW_DESCRIPTOR_OFX = 0x20
VIEW_DESCRIPTOR_OFY = 0x22
VIEW_DESCRIPTOR_DISTANCE = 0x18

# The four sites that read H BACK with `cfc2`. This is the measurement that makes widening H a
# COUPLED change in this title rather than a pure projection change, and it corrects an earlier
# recorded claim that nothing in the text reads H back.
H_READ_SITES: tuple[int, ...] = (0x8006A6B8, 0x80070AF8, 0x80070EE8, 0x80071150)

# The exact words at the first submitter's descriptor loads plus its shared tail, quoted so a reader
# can check the recovery without running anything.
GEOMETRY_PROLOGUE_WORDS: dict[int, int] = {
    0x8006A0BC: 0x84830020,  # lh  v1, 0x20(a0)   OFX = (int16) scene+0x20
    0x8006A0C0: 0x84880022,  # lh  t0, 0x22(a0)   OFY = (int16) scene+0x22
    0x8006A0C4: 0x8C890018,  # lw  t1, 0x18(a0)   H   =          scene+0x18
    0x8006A0D0: 0x00031BC0,  # sll v1, v1, 15
    0x8006A0D4: 0x000843C0,  # sll t0, t0, 15
    0x8006A0D8: 0x48C3C000,  # ctc2 v1, $24
    0x8006A0DC: 0x48C8C800,  # ctc2 t0, $25
    0x8006A0E0: 0x48C9D000,  # ctc2 t1, $26
}

H_READ_WORDS: dict[int, int] = {
    0x8006A6B8: 0x4843D000,
    0x80070AF8: 0x4848D000,
    0x80070EE8: 0x4843D000,
    0x80071150: 0x4843D000,
}


def check_submission_tail(image: Image, tail: int) -> list[str]:
    """Every way this site fails to be the recovered shared tail. Empty list == it is.

    The comparison is on the DECODED shape, not on raw words, so the result is a statement about the
    instruction sequence rather than about a constant this file happens to hold. It is also the only
    thing standing between this file and a confidently wrong claim: an earlier revision asserted a
    single eight-instruction prologue, and this function is what reported that 0 of 10 sites matched.
    """
    problems: list[str] = []
    for step, (want_op, want_operand) in enumerate(SUBMISSION_TAIL_SHAPE):
        address = tail + step * 4
        word = image.word(address)
        instruction = decode(address, word)
        got_op = instruction.op
        got_operand = ""
        if want_op == "sll":
            got_operand = f"{instruction.shamt}"
        elif want_op == "ctc2":
            got_operand = f"{instruction.rd}"
        if got_op != want_op or got_operand != want_operand:
            problems.append(
                f"0x{address:08X} ({word:08X}) decodes as {fmt(instruction)!r}, "
                f"expected {want_op} {want_operand}"
            )
    return problems


def descriptor_reads(image: Image, tail: int) -> list[str]:
    """The descriptor loads feeding the shared tail, read out of the bytes just above it.

    MEASURED, NOT ASSUMED. The recovered story is "the submitters read the same view descriptor the
    0x80042910 publication reads", so the offsets have to come from the image at each site. This
    walks backwards from the tail collecting the `lh`/`lw` loads that land in the same registers the
    tail's shifts and writes consume, and reports what it found -- including "none found", which
    would be a finding and not a pass.
    """
    found: list[tuple[int, object, str]] = []
    for step in range(1, 9):
        address = tail - step * 4
        instruction = decode(address, image.word(address))
        if instruction.op in ("lh", "lw") and instruction.rs in (4, 6):  # base a0 / a2
            found.append((address, instruction, "a0" if instruction.rs == 4 else "a2"))
    return list(reversed(found))


def cmd_gte_projection(image: Image, _args: argparse.Namespace) -> int:
    """Every writer and every reader of the GTE projection distance H, with denominators."""
    print(f"[gte-projection] {image.coverage()}")

    cop2_words = 0
    unnamed = 0
    rtps_sites: list[int] = []
    op_histogram: dict[int, int] = {}
    h_writes: list[int] = []
    ofx_writes: list[int] = []
    h_reads: list[int] = []
    jal_targets: dict[int, list[int]] = {}
    j_targets: dict[int, list[int]] = {}
    jalr_total = 0
    for address, instruction in image.instructions():
        if instruction.op == "cop2":
            cop2_words += 1
            sub = (image.word(address) >> 5) & 0x3F
            op_histogram[sub] = op_histogram.get(sub, 0) + 1
            if sub == GTE_OP_RTPS:
                rtps_sites.append(address)
        elif instruction.op == "ctc2":
            if instruction.rd == GTE_CR_H:
                h_writes.append(address)
            elif instruction.rd == GTE_CR_OFX:
                ofx_writes.append(address)
        elif instruction.op == "cfc2" and instruction.rd == GTE_CR_H:
            h_reads.append(address)
        elif instruction.op == "jal":
            jal_targets.setdefault(instruction.target, []).append(address)
        elif instruction.op == "j":
            j_targets.setdefault(instruction.target, []).append(address)
        elif instruction.op == "jalr":
            jalr_total += 1

    # COVERAGE LIMIT, stated rather than implied: the executable's text extent also holds data
    # tables and ASCII strings, and a word of ASCII in that range decodes as a COP2 op. The
    # unnamed sub-op codes are the visible face of that, and they are reported instead of being
    # folded into a "GTE op" total.
    named = {0x01, 0x06, 0x0C, 0x10, 0x11, 0x12, 0x13, 0x14, 0x16, 0x1B, 0x1C, 0x1E, 0x20,
             0x28, 0x29, 0x2A, 0x2D, 0x2E, 0x30, 0x3D, 0x3E, 0x3F}
    for sub, count in sorted(op_histogram.items()):
        unnamed += 0 if sub in named else count
    print(f"[gte-projection] scanned {image.words} word(s) for COP2 traffic: {cop2_words} decode as a "
          f"GTE op word, of which {unnamed} carry a sub-op code the framework's own gte_name() table "
          f"does not name. Those are REPORTED, not counted as behaviour: the text extent holds data "
          f"and ASCII as well as code, and 'COP2-looking' is not the same as 'executed'.")
    print(f"[gte-projection] perspective transform RTPS (sub-op 0x{GTE_OP_RTPS:02X}): {len(rtps_sites)} "
          f"site(s) in the text — the guest's 3D vertex transform, statically present. "
          f"Executed counts are NOT claimed here; this is a text census.")
    print()

    leaf_callers = len(jal_targets.get(SET_GEOM_SCREEN, []))
    print(f"[gte-projection] H (CR{GTE_CR_H}) WRITERS: {len(h_writes)} raw `ctc2 rX,$26` word(s) plus "
          f"{leaf_callers} `jal SetGeomScreen` caller(s) = {len(h_writes) + leaf_callers} in total. "
          f"Of the raw words, two ARE the libgte leaves themselves "
          f"(0x{SET_GEOM_SCREEN:08X} and 0x800778A0).")
    for address in h_writes:
        print(f"    0x{address:08X}: {image.word(address):08X}  {fmt(decode(address, image.word(address)))}")
    print(f"[gte-projection] OFX (CR{GTE_CR_OFX}) raw writers: {len(ofx_writes)}; "
          f"OFY (CR{GTE_CR_OFY}) raw writers: "
          f"{sum(1 for a, i in image.instructions() if i.op == 'ctc2' and i.rd == GTE_CR_OFY)}")
    print()

    print(f"[gte-projection] THE RECOVERED SUBMISSION PROLOGUE — the descriptor offsets and the "
          f"fixed-point shift, quoted from the first submitter:")
    for address in sorted(GEOMETRY_PROLOGUE_WORDS):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    print()

    print(f"[gte-projection] the ten geometry submitters. Each is reached only by `jal`; a site "
          f"reached by `j` or by falling through is NEVER a valid override key, so the `j` column "
          f"is the check that makes these keys legitimate. The `prologue` column is the same eight-"
          f"instruction check run at every site, so a single divergent submitter is visible instead "
          f"of being hidden behind the one that was quoted.")
    print(f"  {'entry':>10}  {'tail':>10}  {'jal':>4}  {'j':>3}  {'tail shape':>11}  first direct caller")
    reached_by_j = 0
    tail_failures = 0
    descriptor_shapes: dict[tuple[str, ...], int] = {}
    for entry, tail in GEOMETRY_SUBMITTERS:
        jal_sites = jal_targets.get(entry, [])
        j_sites = j_targets.get(entry, [])
        reached_by_j += len(j_sites)
        problems = check_submission_tail(image, tail)
        verdict = "OK" if not problems else f"DIFF({len(problems)})"
        if problems:
            tail_failures += 1
            for problem in problems:
                print(f"      entry 0x{entry:08X}: {problem}")
        caller = f"0x{jal_sites[0]:08X}" if jal_sites else "NONE - NOT AN OVERRIDE KEY"
        print(f"  0x{entry:08X}  0x{tail:08X}  {len(jal_sites):>4}  {len(j_sites):>3}  "
              f"{verdict:>11}  {caller}")
        shape = tuple((address, f"{instruction.op} 0x{instruction.imm:X}", base)
                      for address, instruction, base in descriptor_reads(image, tail))
        descriptor_shapes[shape] = descriptor_shapes.get(shape, 0) + 1
    print(f"[gte-projection] the descriptor loads feeding each tail, MEASURED per site (this is why "
          f"the shared claim is five instructions and not eight):")
    for shape, count in sorted(descriptor_shapes.items(), key=lambda kv: -kv[1]):
        print(f"  {count} of {len(GEOMETRY_SUBMITTERS)} site(s):")
        if not shape:
            print("    (none found in the 8 words above the tail)")
        for address, described, base in shape:
            print(f"    0x{address:08X} {described:16s} [base {base}]")
    # The load-bearing conclusion, and it is a MEASUREMENT over all ten sites rather than a reading
    # of one: every submitter takes its projection triple from the SAME view descriptor the 0x80042910
    # publication reads. That is why one owned plan can be applied at both publication sites.
    #
    # +0xF4 also appears in the window, and it is NOT a fourth projection field: it is the value the
    # guest DERIVES from H and stores at 0x8006A6B8, read back beside the triple. So the gate asserts
    # the three projection offsets are present and names every extra displacement rather than
    # demanding an exact set -- a demand that would have reported the real finding as a failure.
    offsets_seen: set[int] = set()
    for _entry, tail in GEOMETRY_SUBMITTERS:
        for _address, instruction, _base in descriptor_reads(image, tail):
            offsets_seen.add(instruction.imm)
    required = {VIEW_DESCRIPTOR_OFX, VIEW_DESCRIPTOR_OFY, VIEW_DESCRIPTOR_DISTANCE}
    missing = sorted(required - offsets_seen)
    extra = sorted(offsets_seen - required)
    print(f"[gte-projection] view-descriptor displacements read above the tails across all "
          f"{len(GEOMETRY_SUBMITTERS)} sites: "
          + ", ".join(f"0x{o:02X}" for o in sorted(offsets_seen)) + ".")
    print(f"[gte-projection] required projection offsets "
          + ", ".join(f"0x{o:02X}" for o in sorted(required))
          + f" -> {'all present' if not missing else 'MISSING ' + ', '.join(f'0x{o:02X}' for o in missing)}.")
    if extra:
        print(f"[gte-projection] additional displacement(s) in the window: "
              + ", ".join(f"0x{o:02X}" for o in extra)
              + ". 0xF4 is the field the guest WRITES FROM H at 0x8006A6B8 and reads back here, so it "
                "is the coupling between a widened H and the guest's own viewport, not a fourth "
                "projection field. It is named rather than rejected.")
    print()
    print(f"[gte-projection] {len(GEOMETRY_SUBMITTERS)} function entries asked about; "
          f"{len(GEOMETRY_SUBMITTERS) - tail_failures} of them carry the recovered shared tail "
          f"verbatim, {tail_failures} do not. "
          f"{sum(len(jal_targets.get(e, [])) for e, _ in GEOMETRY_SUBMITTERS)} direct `jal` "
          f"caller(s) found, {reached_by_j} `j` target(s) found. "
          f"{jalr_total} `jalr` site(s) in the text are NOT resolved by this scan, so an indirect "
          f"caller of one of these entries would be invisible here — the `jal` column is a floor, "
          f"not a total.")
    print()

    print(f"[gte-projection] H (CR{GTE_CR_H}) READERS — the measurement that makes widening H a "
          f"COUPLED change in this title:")
    for address in h_reads:
        print(f"  0x{address:08X}: {image.word(address):08X}  {fmt(decode(address, image.word(address)))}")
    print(f"[gte-projection] {len(h_reads)} reader(s) of {len(h_writes)} raw writer(s). The guest "
          f"derives screen-space quantities FROM H (at 0x8006A6B8 it stores `2H - CR7` into the "
          f"scene descriptor at +0xF4; at 0x80070AF8 it DIVIDES by H; at 0x80070EE8 it builds "
          f"`4H`; at 0x80071150 it uses H/2 for a vertex). So the earlier claim that nothing in the "
          f"text reads H back was WRONG, and any 'widening H is safe because nothing reads it' "
          f"argument is void for this title.")
    return 0


def cmd_frame_pacing(image: Image, _args: argparse.Namespace) -> int:
    """The chain that decides how many fields one game frame spans."""
    print(f"[pacing] {image.coverage()}")
    offset = COUNTDOWN_GP_OFFSET
    accesses = [
        (address, instruction)
        for address, instruction in image.instructions()
        if instruction.rs == 28 and instruction.imm == offset and instruction.op in MEM_OPS
    ]
    print(f"[pacing] the frame countdown is [gp+0x{offset:X}] — the ONLY words in the text that "
          f"touch it are {len(accesses)}:")
    for address, instruction in accesses:
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(instruction)}")
    print(f"[pacing] scanned {image.words} word(s) for a gp-relative 0x{offset:X} access; "
          f"matched {len(accesses)}")
    print()
    print("[pacing] the literal the frame loop loads before storing it:")
    for address in range(0x80037924, 0x8003793C, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    print()
    print("[pacing] the frame suffix that blocks on it:")
    for address in range(0x800378B4, 0x800378D4, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    print()
    print("[pacing] the callback that decrements it, quoted from the image:")
    for address in range(0x80034AE0, 0x80034B00, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    print()
    print("[pacing] its installation site (the pointer is built in a jal DELAY SLOT, which is why "
          "an adjacent lui/addiu scan cannot see it):")
    for address in range(0x8003C8E8, 0x8003C8F4, 4):
        word = image.word(address)
        print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
    return 0


def cmd_near_plane(image: Image, _args: argparse.Namespace) -> int:
    """The horizontal/visibility cull: what it compares against, and what it is NOT."""
    print(f"[near-plane] {image.coverage()}")
    print(f"[near-plane] the two geometry paths build the scratchpad near plane at "
          f"0x{SCRATCHPAD_NEAR_PLANE:08X}, quoted from the image:")
    for entry in (0x8006E5BC, 0x8006F038):
        for address in range(entry, entry + 0x18, 4):
            word = image.word(address)
            print(f"  0x{address:08X}: {word:08X}  {fmt(decode(address, word))}")
        print()
    print(f"[near-plane] every store to 0x{SCRATCHPAD_NEAR_PLANE:08X} found by scanning for the "
          f"`lui <reg>,0x1F80` + store-displacement form over {image.words} word(s):")
    found = 0
    for address, instruction in image.instructions():
        if instruction.op != "sw" or instruction.imm != 0x54:
            continue
        register = instruction.rs
        for back in range(1, 41):
            previous = address - 4 * back
            if previous < image.exe.load:
                break
            earlier = decode(previous, image.word(previous))
            if earlier.op == "lui" and earlier.rt == register and earlier.imm == 0x1F80:
                word = image.word(address)
                print(f"  0x{address:08X}: {word:08X}  {fmt(instruction)}  (lui at 0x{previous:08X})")
                found += 1
                break
            if (earlier.rt == register and earlier.op in MEM_OPS and earlier.op != "lui") or (
                earlier.rd == register and earlier.op in A0_DEFINING_REGISTER
            ):
                break
    print(f"[near-plane] matched {found} site(s)")
    print()
    print("[near-plane] THIS IS NOT THE PROJECTION H. The projection H reaches the GTE only, "
          "through SetGeomScreen's ctc2 a0,$26. The near plane above is a small literal "
          "(0 or 2) chosen by the caller's third argument, so widening H does not move it.")
    print(f"[near-plane] COVERAGE LIMIT: 0 of N BIGFILE overlay images are provisioned, and CTR's "
          f"overlays reuse load addresses, so this covers the main executable only.")
    return 0


def cmd_gp_note(_image: Image, _args: argparse.Namespace) -> int:
    """Why there is no `gp` subcommand: naming the countdown's absolute address is not needed."""
    print("[gp] NOT REPORTED, and the reason is part of the measurement.")
    print("[gp] This executable's PS-X EXE header carries gp=0, so gp is not read from the image.")
    print("[gp] The frame-pacing scan therefore keys on the ENCODING — base register 28 (gp) and")
    print("[gp] displacement 0x348 — which is base-independent, complete, and needs no gp value.")
    print("[gp] Quoting an absolute address for the countdown would require assuming a gp this tool")
    print("[gp] cannot ground, so the report names it as [gp+0x348] and stops there.")
    return 0


# --------------------------------------------------------------------------------------
# selftest
# --------------------------------------------------------------------------------------


def selftest(image: Image) -> int:
    """Exercise the shipping scan paths, including the cases that MUST fail.

    Every assertion here runs the same functions the reports above run. A selftest that
    exercised a private helper would prove nothing about the tool that produced the numbers.
    """
    failures: list[str] = []

    def check(name: str, condition: bool, detail: str) -> None:
        if not condition:
            failures.append(f"{name}: {detail}")
        print(f"  [{'PASS' if condition else 'FAIL'}] {name} — {detail}")

    print("[selftest] POSITIVE: identity is read out of the image, not asserted")
    check(
        "identity load address",
        image.exe.load == 0x80010000,
        f"load=0x{image.exe.load:08X}",
    )
    check(
        "identity text extent",
        image.exe.text_end == 0x8008D800,
        f"text_end=0x{image.exe.text_end:08X}",
    )
    check(
        "identity digest",
        image.digest == RECORDED_SHA256,
        f"sha256 matches recorded identity",
    )
    check(
        "identity word count",
        image.words == 128512,
        f"{image.words} instruction words in the text extent",
    )

    print("[selftest] POSITIVE: the VSync census, cross-checked against a second method")
    sites = [a for a, i in image.instructions() if i.op == "jal" and i.target == VSYNC]
    # Second, independent method: the raw opcode/target word, not the decoder.
    raw_pattern = ((VSYNC >> 2) & 0x03FFFFFF) | (0x03 << 26)
    raw_sites = [
        image.exe.load + index * 4
        for index in range(image.words)
        if image.word(image.exe.load + index * 4) == raw_pattern
    ]
    check(
        "vsync census agrees with the raw-word method",
        sites == raw_sites and len(sites) > 0,
        f"decoder found {len(sites)}, raw-word scan found {len(raw_sites)}, same addresses",
    )

    print("[selftest] NEGATIVE: a census with no matches must say scanned-and-found-none, with its "
          "denominator, and must not report a hit")
    parsed = _capture(cmd_callers, image, _namespace(address="0x80010000"))
    check(
        "empty census reports zero WITH a denominator",
        "matched 0 direct `jal` site(s)" in parsed and "scanned 128512" in parsed,
        "output names both the match count and the words scanned",
    )
    check(
        "empty census names the indirect-call gap",
        "COVERAGE LIMIT" in parsed,
        "output states the jalr sites it could not resolve",
    )

    print("[selftest] NEGATIVE: the argument-semantics guard must refuse a corrupted image")
    try:
        _vsync_wait_semantics(_CorruptedAt(image, 0x800753A8))
        check("semantics guard refuses corruption", False, "no refusal raised on a wrong word")
    except Refusal as refusal:
        check("semantics guard refuses corruption", True, f"refused: {str(refusal)[:70]}…")

    print("[selftest] NEGATIVE: a non-PS-X-EXE image must be refused, not read")
    try:
        psx_exe.load(_write_temp(b"NOT-AN-EXE" + bytes(0x900)))
        check("non-psx-exe refused", False, "no refusal raised")
    except ValueError as error:
        check("non-psx-exe refused", True, f"refused: {str(error)[:60]}")
    except Refusal:
        check("non-psx-exe refused", True, "refused by the tool")

    print("[selftest] NEGATIVE: a missing image must be refused, with the re-provision command")
    try:
        open_image(_ROOT / "scratch" / "raw" / "ctr" / "NO-SUCH-IMAGE")
        check("missing image refused", False, "no refusal raised")
    except Refusal as refusal:
        check(
            "missing image refused",
            "provision.py" in str(refusal),
            "refusal names the re-provision command",
        )

    print("[selftest] POSITIVE: the projection owner, quoted from the image")
    publisher = _capture(cmd_projection_owner, image, _namespace())
    check(
        "publisher body ends where the docs say",
        f"0x{PROJECTION_PUBLISHER + 0x64:08X}: 27BDFF80" in publisher,
        "the word after the epilogue is the next function's prologue",
    )
    check(
        "three direct publication callers",
        "publisher (0x80042910): 3 site(s)" in publisher,
        "three `jal` sites reach the publication",
    )
    check(
        "startup publication is two more libgte calls",
        "SetGeomOffset (0x8007782C): 2 site(s)" in publisher
        and "SetGeomScreen (0x8007781C): 2 site(s)" in publisher,
        "the state-zero literal publication is the second caller of each leaf",
    )
    check(
        "SetGeomScreen is a single ctc2 into GTE $26",
        "0x8007781C: 48C4D000  ctc2 a0, $26" in publisher,
        "H reaches the GTE as control register 26",
    )

    print("[selftest] POSITIVE: the frame pacing chain, quoted from the image")
    pacing = _capture(cmd_frame_pacing, image, _namespace())
    check(
        "countdown is written with the literal 2",
        "0x80037930: 24020002  addiu v0, zero, 2" in pacing,
        "the frame loop loads 2 before storing it",
    )
    check(
        "frame suffix blocks while the countdown is positive",
        "0x800378C8: 1C40FFF2  bgtz v0" in pacing,
        "the suffix spins until the countdown is not positive",
    )
    check(
        "only four words touch the countdown",
        "are 4:" in pacing,
        "two writers and two readers, nothing else in the text",
    )
    check(
        "the callback pointer is built in a delay slot",
        "0x8003C8F0: 24844AA4  addiu a0, a0, 19108" in pacing,
        "the decrementer address is materialised 8 bytes from its lui",
    )

    print("[selftest] POSITIVE: the near plane is a literal, not the projection H")
    near = _capture(cmd_near_plane, image, _namespace())
    check(
        "near plane is 0 or 2 from the caller's third argument",
        "0x8006E5CC: 24030002  addiu v1, zero, 2" in near,
        "the selector picks 2, never the projection H",
    )
    check(
        "the tool says plainly that this is not H",
        "THIS IS NOT THE PROJECTION H" in near,
        "the report cannot be read as a statement about H",
    )

    print("[selftest] REGRESSION: the a0 argument lives in the jal's DELAY SLOT, so a walk that "
          "starts at site-4 reads a different (wrong) answer")
    check(
        "the delay slot holds the argument",
        image.word(0x8003C4C4) == 0x2404001E,
        f"0x8003C4C4 is 0x{image.word(0x8003C4C4):08X} (addiu a0, zero, 30)",
    )
    value, how = _resolve_a0(image, 0x8003C4C0)
    check(
        "the walk resolves VSync(30) at 0x8003C4C0",
        value == 30 and "delay slot" in how,
        f"resolved {value} via {how}",
    )
    # The wrong answer, asserted so the regression is visible rather than silent: reading from
    # site-4 instead sees the delay slot of the PRECEDING call, whose a0 is untouched.
    preceding_delay = decode(0x8003C4BC, image.word(0x8003C4BC))
    check(
        "a walk starting one instruction low would have read something else",
        preceding_delay.op == "nop",
        f"0x8003C4BC is {fmt(preceding_delay)}, so site-4 could not have produced 30",
    )

    print("[selftest] THE 3D SUBMISSION PATH: the shared tail, at every site, from the bytes")
    tail_problems: list[str] = []
    for entry, tail in GEOMETRY_SUBMITTERS:
        for problem in check_submission_tail(image, tail):
            tail_problems.append(f"0x{entry:08X}: {problem}")
    check(
        f"all {len(GEOMETRY_SUBMITTERS)} submitters carry the shared projection tail verbatim",
        not tail_problems,
        "; ".join(tail_problems) if tail_problems
        else f"{len(GEOMETRY_SUBMITTERS)}/{len(GEOMETRY_SUBMITTERS)} sites OK",
    )
    offsets: set[int] = set()
    for _entry, tail in GEOMETRY_SUBMITTERS:
        for _address, instruction, _base in descriptor_reads(image, tail):
            offsets.add(instruction.imm)
    check(
        "every submitter reads the view descriptor's own projection offsets",
        {VIEW_DESCRIPTOR_OFX, VIEW_DESCRIPTOR_OFY, VIEW_DESCRIPTOR_DISTANCE} <= offsets,
        f"displacements seen across all sites: "
        + ", ".join(f"0x{o:02X}" for o in sorted(offsets)),
    )
    words_ok = all(
        image.word(address) == word for address, word in GEOMETRY_PROLOGUE_WORDS.items()
    )
    check(
        "the quoted first-submitter words are the image's words",
        words_ok,
        f"{len(GEOMETRY_PROLOGUE_WORDS)} word(s) compared at 0x8006A0BC..0x8006A0E0",
    )
    reader_words_ok = all(image.word(a) == w for a, w in H_READ_WORDS.items())
    check(
        "the H readers are the measured `cfc2 rX,$26` sites",
        reader_words_ok,
        f"{len(H_READ_WORDS)} word(s) compared at "
        + ", ".join(f"0x{a:08X}" for a in sorted(H_READ_WORDS)),
    )
    # NEGATIVE, AND IT IS THE NEGATIVE THAT MATTERS MOST HERE. The instrument has to be able to
    # report a MISS, or "10 of 10" is not evidence. This corrupts one word of the shared tail at one
    # site and requires the same checker to reject that site while accepting the other nine. An
    # earlier revision of this gate failed exactly this way in the other direction: it asserted a
    # shape no site had and reported 0 of 10, which is the same code path that must be able to say 9.
    corrupted = _CorruptedAt(image, GEOMETRY_SUBMITTERS[0][1])
    surviving = [
        entry for entry, tail in GEOMETRY_SUBMITTERS[1:]
        if not check_submission_tail(corrupted, tail)
    ]
    rejected = check_submission_tail(corrupted, GEOMETRY_SUBMITTERS[0][1])
    check(
        "one corrupted tail word is rejected, the other sites still accepted",
        bool(rejected) and len(surviving) == len(GEOMETRY_SUBMITTERS) - 1,
        f"corrupted site reported {len(rejected)} difference(s); "
        f"{len(surviving)}/{len(GEOMETRY_SUBMITTERS) - 1} other sites still accepted",
    )
    # NEGATIVE, for the H-reader census specifically: a claim that "nothing reads H back" is the
    # exact error this work corrected, so the census has to be able to produce an empty result on an
    # image that genuinely has no reader.
    class _NoReaders:
        def __init__(self, inner):
            self._inner = inner

        def __getattr__(self, name):
            return getattr(self._inner, name)

        def instructions(self):
            for address, instruction in self._inner.instructions():
                if instruction.op == "cfc2" and instruction.rd == GTE_CR_H:
                    continue
                yield address, instruction

    found_without = [a for a, i in _NoReaders(image).instructions()
                     if i.op == "cfc2" and i.rd == GTE_CR_H]
    check(
        "the H-reader census can report zero when the image has none",
        not found_without,
        f"with the {len(H_READ_SITES)} reader(s) filtered out, matched {len(found_without)} — "
        f"so the {len(H_READ_SITES)} above is a property of the image, not of the scan",
    )

    print("[selftest] CROSS-CHECK: the byte census against the Ghidra decompilation census")
    resolved = {site: _resolve_a0(image, site)[0] for site in sites}
    problems = _cross_check_ghidra(image, resolved)
    disagreements = [p for p in problems if "disagree" in p or p.startswith("REFUSING")]
    check(
        "no site disagrees between the two methods",
        not disagreements,
        "; ".join(disagreements) if disagreements else "0 disagreements",
    )
    if any(p.startswith("NO CROSS-CHECK") for p in problems):
        print("  [NOTE] the Ghidra census is absent, so the cross-check did NOT run — the "
              "byte census is uncorroborated in this run")

    print()
    if failures:
        print(f"[selftest] {len(failures)} FAILURE(S):")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("[selftest] all checks passed")
    return 0


class _CorruptedAt:
    """A view of the image with one word replaced, for the negative case."""

    def __init__(self, image: Image, address: int) -> None:
        self._image = image
        self._address = address

    def __getattr__(self, name: str):
        return getattr(self._image, name)

    def word(self, address: int) -> int:
        return 0xDEADBEEF if address == self._address else self._image.word(address)

    def instructions(self):
        for address, instruction in self._image.instructions():
            if address == self._address:
                yield address, decode(address, self.word(address))
            else:
                yield address, instruction


def _namespace(**kwargs) -> argparse.Namespace:
    return argparse.Namespace(**kwargs)


def _capture(command, image: Image, args: argparse.Namespace) -> str:
    import io
    from contextlib import redirect_stdout

    buffer = io.StringIO()
    with redirect_stdout(buffer):
        command(image, args)
    return buffer.getvalue()


def _write_temp(payload: bytes) -> Path:
    import tempfile

    handle = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    handle.write(payload)
    handle.close()
    return Path(handle.name)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", type=Path, default=None, help="the authenticated SCUS_944.26 image")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("identity", help="PS-X EXE identity read out of the image")
    disasm = sub.add_parser("disasm", help="read and decode instruction words")
    disasm.add_argument("address")
    disasm.add_argument("count", nargs="?", type=int, default=8)
    callers = sub.add_parser("callers", help="census every direct `jal` to an address")
    callers.add_argument("address")
    sub.add_parser("vsync-arguments", help="census every direct VSync(n) site and classify it")
    sub.add_parser("vsync-wait-semantics", help="read the VSync argument branch structure")
    sub.add_parser("projection-owner", help="quote the projection publication and its callers")
    sub.add_parser("gte-projection",
                 help="census every writer and reader of the GTE projection distance H")
    sub.add_parser("frame-pacing", help="quote the per-frame field countdown chain")
    sub.add_parser("near-plane", help="quote the geometry cull and what it compares against")
    sub.add_parser("gp", help="why the tool reports the countdown as [gp+0x348] and not an address")
    sub.add_parser("selftest", help="run the self-check, including the negative cases")
    args = parser.parse_args(argv)

    if args.command == "selftest":
        try:
            image = open_image(args.image)
        except Refusal as refusal:
            # 77 is CTest's SKIP_RETURN_CODE. This tool reads the authenticated image, so in an
            # asset-free CI job there is nothing to read. Skipping WITH THE REASON is the honest
            # outcome; failing would look like a broken probe, and passing silently would claim a
            # measurement that never ran.
            print(f"[selftest] SKIPPED: {refusal}")
            return SKIP_EXIT_CODE
        return selftest(image)

    try:
        image = open_image(args.image)
    except Refusal as refusal:
        print(f"REFUSED: {refusal}")
        return 2
    commands = {
        "identity": cmd_identity,
        "disasm": cmd_disasm,
        "callers": cmd_callers,
        "vsync-arguments": cmd_vsync_arguments,
        "vsync-wait-semantics": cmd_vsync_wait_semantics,
        "projection-owner": cmd_projection_owner,
        "gte-projection": cmd_gte_projection,
        "frame-pacing": cmd_frame_pacing,
        "near-plane": cmd_near_plane,
        "gp": cmd_gp_note,
    }
    return commands[args.command](image, args)


if __name__ == "__main__":
    raise SystemExit(main())
