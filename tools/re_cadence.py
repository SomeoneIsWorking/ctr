#!/usr/bin/env python3
"""CTR's per-frame field count, and why its single VSync(2) is NOT the frame rate.

The workspace map recorded "`ctr`: `VSync(2)` x1, plus 0 and -1 -> leads to 30 fps, one site
only". The NUMBER was right and the REASONING was wrong, and this tool exists so the reasoning
is measured rather than inherited: the one `VSync(2)` sits in a boot resource load, not in the
frame loop, and the frame loop paces on a two-field countdown drained by the vertical-blank
callback. Both facts are read out of the image here, with denominators.

Reuses `tools/ctr_binary_probe.py`, which already owns this title's PS-X EXE reader, its R3000A
decoder, the image identity assertion and the VSync argument semantics. A second implementation
of any of those is exactly the drift this repository keeps measuring, so this tool adds no
decoder of its own.
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
for _extra in (_ROOT / "external" / "psxport" / "tools", _ROOT / "tools"):
    if str(_extra) not in sys.path:
        sys.path.insert(0, str(_extra))

from formats import psx_exe  # noqa: E402
from mips import decode, fmt  # noqa: E402

import ctr_binary_probe as probe  # noqa: E402

# The frame loop's two-field countdown, the literal it arms it with, the callback that drains
# it, and the suffix that blocks on it. Every word is read back and asserted; these offsets were
# taken from the disassembly, not fitted until they passed.
COUNTDOWN_DISP = 0x348
ARM_LITERAL_SITE = 0x80037930
ARM_LITERAL_WORD = 0x24020002
COUNTDOWN_STORE_WORD = 0xAF820348   # sw v0,840(gp), the instruction AFTER the literal
COUNTDOWN_LOAD_WORD = 0x8F820348    # lw v0,840(gp)
DRAIN_SITE = 0x80034AEC
DRAIN_WORD = 0x2442FFFF
BLOCK_SITE = 0x800378C8
BLOCK_WORD = 0x1C40FFF2
# The one VSync(2), in FUN_80031FDC's `param_5 == -1` boot-resource branch.
VSYNC2_SITE = 0x8003206C
VSYNC2_WORD = 0x0C01D4D4


def measure(image, verify_identity: bool = True) -> dict:
    if verify_identity and image.digest != probe.RECORDED_SHA256:
        raise probe.Refusal(
            f"image SHA-256 {image.digest[:12]} is not the recorded "
            f"{probe.RECORDED_SHA256[:12]}; nothing was measured"
        )
    semantics = probe._vsync_wait_semantics(image)

    for address, word, why in (
        (ARM_LITERAL_SITE, ARM_LITERAL_WORD, "the frame loop arms the countdown with 2"),
        (DRAIN_SITE, DRAIN_WORD, "the vblank callback decrements it by exactly 1"),
        (BLOCK_SITE, BLOCK_WORD, "the frame suffix spins while it is positive"),
    ):
        actual = image.word(address)
        if actual != word:
            raise probe.Refusal(
                f"0x{address:08X} is 0x{actual:08X}, not 0x{word:08X}; {why} is not present, "
                f"so the frame cadence is not reported"
            )

    # The countdown's whole footprint: a claim that it is armed once per frame is only as good
    # as the claim that nothing else writes it, so every access is counted.
    scanned = 0
    accesses = []
    for address, insn in image.instructions():
        scanned += 1
        word = image.word(address)
        if (word & 0xFFFF) != COUNTDOWN_DISP or ((word >> 21) & 31) != 28:
            continue
        accesses.append((address, word, insn))
    if len(accesses) != 4:
        shown = ", ".join(f"0x{a:08X}" for a, _w, _d in accesses) or "none"
        raise probe.Refusal(
            f"the frame countdown [gp+0x{COUNTDOWN_DISP:03X}] has {len(accesses)} accesses, "
            f"not the 4 this cadence rests on ({shown}); refusing to report a rate"
        )
    # ARM_LITERAL_SITE is the `addiu v0,zero,2`; the countdown is stored FOUR BYTES LATER, by
    # the `sw v0,840(gp)` in the following instruction. Asserting the store is at the literal
    # is an off-by-one that reads as "the arming literal is missing" -- a correct refusal for
    # the wrong reason, which is why the store word is checked at its own address.
    if image.word(ARM_LITERAL_SITE + 4) != COUNTDOWN_STORE_WORD:
        raise probe.Refusal(
            f"0x{ARM_LITERAL_SITE + 4:08X} is 0x{image.word(ARM_LITERAL_SITE + 4):08X}, not "
            f"0x{COUNTDOWN_STORE_WORD:08X}; the literal 2 is not stored to the countdown"
        )
    if image.word(DRAIN_SITE - 8) != COUNTDOWN_LOAD_WORD or image.word(DRAIN_SITE) != DRAIN_WORD:
        raise probe.Refusal("the countdown is not read and decremented by exactly 1 per field")
    if image.word(BLOCK_SITE - 8) != COUNTDOWN_LOAD_WORD:
        raise probe.Refusal("the frame suffix does not read the countdown it blocks on")

    # The VSync(2) site is a boot path, so the frame loop's own only VSync is checked too: the
    # census already says it is VSync(0), and that is the site a reader would point at.
    if image.word(VSYNC2_SITE) != VSYNC2_WORD:
        raise probe.Refusal(
            f"0x{VSYNC2_SITE:08X} is 0x{image.word(VSYNC2_SITE):08X}, not the jal the census "
            f"records as the single VSync(2)"
        )
    frame_loop_vsync = 0x80037878
    if image.word(frame_loop_vsync) != 0x0C01D4D4:
        raise probe.Refusal("the frame loop's VSync site moved; the census must be redone")
    delay = image.word(frame_loop_vsync + 4)
    if delay != 0x00002021:
        raise probe.Refusal(
            f"the frame loop's VSync delay slot is 0x{delay:08X}, not `addu a0,zero,zero`; "
            f"it is no longer VSync(0) and the 'not a field count' claim must be redone"
        )

    return {
        "semantics": semantics,
        "countdown": COUNTDOWN_DISP,
        "accesses": accesses,
        "scanned": scanned,
        "vsync2": VSYNC2_SITE,
        "frame_loop_vsync": frame_loop_vsync,
    }


def report(m) -> None:
    s = m["semantics"]
    print("== CTR field cadence, measured from SCUS_944.26 ==")
    print(f"  VSync argument semantics, read from the image: a0<0 no wait, a0==1 no wait, "
          f"a0<={s.wait_floor} no field wait, a0>={s.wait_first} waits (a0-"
          f"{s.field_offset}) fields")
    print()
    print(f"  The frame countdown is [gp+0x{m['countdown']:03X}]. Every access in the text "
          f"(scanned {m['scanned']} words, matched {len(m['accesses'])}):")
    for address, word, insn in m["accesses"]:
        print(f"    0x{address:08X}  {word:08X}  {fmt(insn)}")
    print()
    print("  The frame loop arms it with the LITERAL 2 (0x80037930) and the vertical-blank")
    print("  callback drains exactly 1 per field (0x80034AEC). The suffix blocks while it is")
    print("  positive (0x800378C8).")
    print()
    print("  FIELDS PER GAME FRAME = 2  ->  30 game frames/s on 60 Hz NTSC (~59.94 fields/s).")
    print("  NOT 60 fps.")
    print()
    print("  WHY NOT FROM VSync. The image's single VSync(n>=2) site is")
    print(f"  0x{m['vsync2']:08X}, inside FUN_80031FDC's `param_5 == -1` branch -- a boot")
    print("  resource load, not the frame loop. The frame loop's only VSync is")
    print(f"  0x{m['frame_loop_vsync']:08X}, whose delay slot is `addu a0,zero,zero`: VSync(0),")
    print("  which does not wait and carries no rate information.")
    print()
    print("  The rest of the census is 21x VSync(-1) (field-clock query), 6x VSync(0) and")
    print("  4x VSync(30). The VSync(30) sites wait 29 fields each and are 1-second-ish")
    print("  timeouts, not the frame cadence.")


def selftest(image) -> int:
    """Each negative breaks ONE measured fact and demands a refusal that names the SUBJECT.

    Asserting only that *something* refused is how a selftest ends up green while testing the
    identity gate six times over, so every case requires the message to mention the site it
    broke.
    """
    import struct

    print("== re_cadence (ctr) selftest ==")
    checks = 0
    m = measure(image)
    print(f"  [ ok ] positive: countdown [gp+0x{m['countdown']:03X}] has exactly "
          f"{len(m['accesses'])} accesses, armed with 2 and drained by 1")
    checks += 1

    # The corruption view is the probe's own `_CorruptedAt`, reused rather than reimplemented.
    # It replaces ONE word with 0xDEADBEEF, which is enough to break any of the exact-word
    # assertions and keeps the negative honest: no temp file, no mutation of the provisioned
    # image, and no second copy of the bytes.
    def destroy(address, why, expect):
        nonlocal checks
        try:
            measure(probe._CorruptedAt(image, address), verify_identity=False)
            raise AssertionError(f"{why} was accepted")
        except probe.Refusal as error:
            if expect not in str(error):
                raise AssertionError(
                    f"{why}: refused for the WRONG reason -- {error!s} does not mention "
                    f"{expect!r}"
                )
            print(f"  [ ok ] {why} refused: {error}")
            checks += 1

    # 1. The frame cadence itself: the literal 2 is the ONLY thing that makes this 30 fps
    #    rather than 60, and a 60 fps title must receive no interpolation at all. This is the
    #    negative that guards the scope answer.
    destroy(ARM_LITERAL_SITE, "the countdown's arming literal", f"0x{ARM_LITERAL_SITE:08X}")
    # 2. The drain step: it is what makes the unit a FIELD rather than an arbitrary tick.
    destroy(DRAIN_SITE, "the countdown's per-field decrement", f"0x{DRAIN_SITE:08X}")
    # 3. The block: without it the loop would not wait at all and the rate would be the CPU's.
    destroy(BLOCK_SITE, "the frame suffix's block on the countdown", f"0x{BLOCK_SITE:08X}")
    # 4. The single VSync(2) site: if it moved, the "it is a boot path, not the frame loop"
    #    claim -- the correction of the workspace map -- would need redoing.
    destroy(VSYNC2_SITE, "the boot-path VSync(2) site", f"0x{VSYNC2_SITE:08X}")
    # 5. The frame loop's VSync(0): if its delay slot became a real wait, the frame loop WOULD
    #    be pacing through VSync and the whole correction would be wrong.
    destroy(m["frame_loop_vsync"] + 4, "the frame loop's VSync becoming a real wait",
            "delay slot")
    # 6. An EXTRA access to the countdown, which is how a 60 fps mode would hide: a second
    #    writer can arm it with anything. The census is 4-wide and must refuse at 5.
    checks += _extra_access_negative(image)

    print(f"ctr re_cadence selftest: {checks}/7 PASS")
    return 0


class _Patched:
    """A view of the image with one word REPLACED by a chosen value.

    Distinct from the probe's `_CorruptedAt`, which can only substitute a poison value. The
    extra-access negative needs a real instruction -- a fifth `sw` to the countdown -- because
    a poison word is not an access at all and the census would correctly ignore it. Substituting
    0xDEADBEEF there would make the negative pass for the wrong reason.
    """

    def __init__(self, image, address, value):
        self._view = probe._CorruptedAt(image, address)
        self._address = address
        self._value = value

    def __getattr__(self, name):
        return getattr(self._view, name)

    def word(self, address):
        return self._value if address == self._address else self._view.word(address)

    def instructions(self):
        for address, insn in self._view.instructions():
            if address == self._address:
                yield address, decode(address, self._value)
            else:
                yield address, insn


def _extra_access_negative(image) -> int:
    # 0x80037938 is `lw a1,7364($a0)` in the frame loop. Rewriting it as `sw v0,840($gp)`
    # adds a FIFTH access to the countdown without changing any of the four asserted words --
    # which is exactly how a 60 fps mode would hide, by arming the countdown from a second
    # place with a 1.
    extra = 0x80037938
    try:
        measure(_Patched(image, extra, COUNTDOWN_STORE_WORD), verify_identity=False)
        raise AssertionError("a fifth access to the frame countdown was accepted")
    except probe.Refusal as error:
        if "not the 4" not in str(error):
            raise AssertionError(f"extra-access negative did not name the count: {error}")
        print(f"  [ ok ] a fifth countdown access refused: {error}")
        return 1


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", default=str(probe.DEFAULT_IMAGE))
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    try:
        image = probe.open_image(args.image)
        if args.selftest:
            return selftest(image)
        report(measure(image))
        return 0
    except (AssertionError, OSError, probe.Refusal) as error:
        print(f"ctr re_cadence REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
