#!/usr/bin/env python3
"""Measure CTR's first binary-grounded projection and primitive-producer boundaries."""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from provision import CTR_USA, ROOT, ProvisionError, verify_executable

LOAD_ADDRESS = CTR_USA.load_address
HEADER_SIZE = 0x800

SETJMP_THUNK = 0x80080260
SET_GEOM_SCREEN = 0x8007781C
SET_GEOM_OFFSET = 0x8007782C
PROJECTION_PRODUCER = 0x80042910
PRIMITIVE_PRODUCER = 0x80024C4C
PRIMITIVE_REGISTRAR = 0x80025138

EXPECTED_WORDS: dict[int, tuple[int, ...]] = {
    SETJMP_THUNK: (0x240A00A0, 0x01400008, 0x24090013),
    SET_GEOM_SCREEN: (0x48C4D000, 0x03E00008, 0x00000000),
    SET_GEOM_OFFSET: (
        0x00042400,
        0x00052C00,
        0x48C4C000,
        0x48C5C800,
        0x03E00008,
        0x00000000,
    ),
    PROJECTION_PRODUCER: (
        0x27BDFFE8,
        0xAFB00010,
        0x00808021,
        0xAFBF0014,
        0x96020020,
        0x00000000,
        0x00021400,
        0x00022403,
        0x000217C2,
        0x00822021,
        0x96020022,
        0x00042043,
        0x00021400,
        0x00022C03,
        0x000217C2,
        0x00A22821,
        0x0C01DE0B,
        0x00052843,
        0x8E040018,
        0x0C01DE07,
        0x00000000,
        0x8FBF0014,
        0x8FB00010,
        0x03E00008,
        0x27BD0018,
    ),
    PRIMITIVE_PRODUCER: (
        0x27BDFFD0,
        0xAFBF002C,
        0xAFB40028,
        0xAFB30024,
        0xAFB20020,
        0xAFB1001C,
        0xAFB00018,
    ),
    # This wrapper passes 0x80024C4C as a callback to 0x8004205C. The nearby retail
    # string is "lensflare"; the static tool deliberately does not infer when it runs.
    PRIMITIVE_REGISTRAR: (
        0x27BDFFE8,
        0xAFB00010,
        0x00808021,
        0x3C04000C,
        0x3484030D,
        0x3C058002,
        0x3C068001,
        0x24A54C4C,
        0x24C61074,
        0xAFBF0014,
        0x0C010817,
        0x00003821,
    ),
    0x80024F28: (0x4A280030,),
    0x80024F98: (0x4A280030,),
    0x80025038: (0x4A280030,),
    # Three linked POLY_GT3-sized packets are tagged 0x0C; the fourth is linked into
    # the ordering table later in the same identity-gated function.
    0x8002509C: (
        0x01441024,
        0x3C050C00,
        0x00451025,
        0xAE220000,
        0x01641024,
        0x00451025,
        0xAE220034,
        0x01841024,
        0x00451025,
        0x05210002,
        0xAE220068,
    ),
}

EXPECTED_CALLERS: dict[int, tuple[int, ...]] = {
    SETJMP_THUNK: (0x80077340,),
    SET_GEOM_SCREEN: (0x8003C858, 0x8004295C),
    SET_GEOM_OFFSET: (0x8003C850, 0x80042950),
    PROJECTION_PRODUCER: (0x80024CCC, 0x8003BD2C, 0x8003F5C0),
    # Its registration wrapper takes the address instead; a direct-caller claim is false.
    PRIMITIVE_PRODUCER: (),
}

EXPECTED_CTC2: dict[int, tuple[int, ...]] = {
    24: (
        0x800445E0,
        0x8004B618,
        0x8004B66C,
        0x8005B948,
        0x8005C3A4,
        0x8006A0D8,
        0x8006AB30,
        0x8006DCDC,
        0x8006E2F8,
        0x8006EB1C,
        0x8006F5A8,
        0x8006FA60,
        0x8006FF20,
        0x80070420,
        0x800709C4,
        0x80077834,
        0x800778C0,
    ),
    25: (
        0x800445E4,
        0x8004B61C,
        0x8004B670,
        0x8005B94C,
        0x8005C3A8,
        0x8006A0DC,
        0x8006AB34,
        0x8006DCE0,
        0x8006E2FC,
        0x8006EB20,
        0x8006F5AC,
        0x8006FA64,
        0x8006FF24,
        0x80070424,
        0x800709C8,
        0x80077838,
        0x800778C4,
    ),
    26: (
        0x800445E8,
        0x8004B54C,
        0x8005B958,
        0x8005C3B4,
        0x8006A0E0,
        0x8006AB38,
        0x8006DCE4,
        0x8006E300,
        0x8006EB24,
        0x8006F5B0,
        0x8006FA68,
        0x8006FF28,
        0x80070428,
        0x800709CC,
        0x8007781C,
        0x800778A0,
    ),
}


class MeasurementError(RuntimeError):
    """The executable bytes do not support the recorded render-frontier claim."""


@dataclass(frozen=True)
class RenderFrontier:
    callers: dict[int, tuple[int, ...]]
    ctc2: dict[int, tuple[int, ...]]
    rtps: tuple[int, ...]
    rtpt: tuple[int, ...]


def address_offset(address: int, size: int) -> int:
    offset = HEADER_SIZE + address - LOAD_ADDRESS
    if offset < HEADER_SIZE or offset + size > HEADER_SIZE + CTR_USA.text_size:
        raise MeasurementError(
            f"address range {address:#010x}+{size:#x} is outside executable text"
        )
    return offset


def words_at(data: bytes, address: int, count: int) -> tuple[int, ...]:
    offset = address_offset(address, count * 4)
    return struct.unpack_from(f"<{count}I", data, offset)


def text_words(data: bytes):
    text = data[HEADER_SIZE : HEADER_SIZE + CTR_USA.text_size]
    for offset in range(0, len(text), 4):
        yield LOAD_ADDRESS + offset, struct.unpack_from("<I", text, offset)[0]


def jal_target(address: int, word: int) -> int | None:
    if word >> 26 != 3:
        return None
    return ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def scan_callers(data: bytes, target: int) -> tuple[int, ...]:
    return tuple(
        address
        for address, word in text_words(data)
        if jal_target(address, word) == target
    )


def scan_ctc2(data: bytes, control_register: int) -> tuple[int, ...]:
    return tuple(
        address
        for address, word in text_words(data)
        if word & 0xFFE007FF == 0x48C00000 and (word >> 11) & 0x1F == control_register
    )


def scan_gte_function(data: bytes, function: int) -> tuple[int, ...]:
    # Ignore the command's lm/sf/translation/vector/matrix modifier bits. The COP2 command bit and
    # six-bit function code identify RTPS/RTPT across every legal modifier combination.
    signature = 0x4A000000 | function
    return tuple(
        address for address, word in text_words(data) if word & 0xFE00003F == signature
    )


def measure(data: bytes) -> RenderFrontier:
    for address, expected in EXPECTED_WORDS.items():
        actual = words_at(data, address, len(expected))
        if actual != expected:
            raise MeasurementError(
                f"retail signature changed at {address:#010x}: "
                f"got {[f'{word:08X}' for word in actual]}, expected {[f'{word:08X}' for word in expected]}"
            )

    callers = {target: scan_callers(data, target) for target in EXPECTED_CALLERS}
    for target, expected in EXPECTED_CALLERS.items():
        if callers[target] != expected:
            raise MeasurementError(
                f"direct JAL callers of {target:#010x} changed: "
                f"got {[f'{address:#010x}' for address in callers[target]]}, "
                f"expected {[f'{address:#010x}' for address in expected]}"
            )

    ctc2 = {register: scan_ctc2(data, register) for register in EXPECTED_CTC2}
    for register, expected in EXPECTED_CTC2.items():
        if ctc2[register] != expected:
            raise MeasurementError(
                f"raw executable CTC2 CR{register} census changed: "
                f"got {[f'{address:#010x}' for address in ctc2[register]]}, "
                f"expected {[f'{address:#010x}' for address in expected]}"
            )

    return RenderFrontier(
        callers=callers,
        ctc2=ctc2,
        rtps=scan_gte_function(data, 0x01),
        rtpt=scan_gte_function(data, 0x30),
    )


def print_report(frontier: RenderFrontier) -> None:
    print("[ctr05] identity-gated static render frontier")
    print(
        "[ctr05] boot next: 0x80080260 -> BIOS A0:13 setjmp; sole direct caller 0x80077340"
    )
    print(
        "[ctr05] SetGeomScreen: [0x8007781C,0x80077828), callers 0x8003C858/0x8004295C"
    )
    print(
        "[ctr05] SetGeomOffset: [0x8007782C,0x80077844), callers 0x8003C850/0x80042950"
    )
    print("[ctr05] boot projection: OFX=256, OFY=120, H=320 at 0x8003C850/0x8003C858")
    print(
        "[ctr05] dynamic projection producer: [0x80042910,0x80042974), view +0x20/+0x22/+0x18"
    )
    print("[ctr05] producer callers: 0x80024CCC, 0x8003BD2C, 0x8003F5C0")
    print(
        "[ctr05] primitive candidate: [0x80024C4C,0x80025138), registered as lensflare callback"
    )
    print(
        "[ctr05] primitive path: MVMVA -> 3 RTPT -> SXY packet writes -> 0x0C OT links"
    )
    print(
        "[ctr05] primitive direct JAL callers: NONE (address taken by registrar 0x80025138)"
    )
    print(
        "[ctr05] raw text-word census: "
        f"CTC2 CR24={len(frontier.ctc2[24])}, CR25={len(frontier.ctc2[25])}, "
        f"CR26={len(frontier.ctc2[26])}; RTPS={len(frontier.rtps)}, RTPT={len(frontier.rtpt)}"
    )
    print(
        "[ctr05] boundary: typed leaf HLE observes only calls through 0x8007781C/0x8007782C; "
        "the other raw CTC2 words and indirect lensflare dispatch require later dynamic proof"
    )


def build_fixture() -> bytes:
    data = bytearray(HEADER_SIZE + CTR_USA.text_size)

    def put(address: int, words: tuple[int, ...]) -> None:
        struct.pack_into(
            f"<{len(words)}I", data, address_offset(address, len(words) * 4), *words
        )

    for register, addresses in EXPECTED_CTC2.items():
        for address in addresses:
            put(address, (0x48C00000 | (register << 11),))
    for address, words in EXPECTED_WORDS.items():
        put(address, words)
    for target, callers in EXPECTED_CALLERS.items():
        target_field = (target >> 2) & 0x03FFFFFF
        for caller in callers:
            put(caller, (0x0C000000 | target_field,))
    return bytes(data)


def selftest() -> None:
    checks = 0
    fixture = build_fixture()
    frontier = measure(fixture)
    checks += 1
    if tuple(len(frontier.ctc2[register]) for register in (24, 25, 26)) != (17, 17, 16):
        raise AssertionError("positive fixture returned the wrong CTC2 census")
    checks += 1

    changed_leaf = bytearray(fixture)
    struct.pack_into("<I", changed_leaf, address_offset(SET_GEOM_SCREEN, 4), 0)
    try:
        measure(bytes(changed_leaf))
    except MeasurementError as error:
        if "signature changed" not in str(error):
            raise AssertionError(f"wrong changed-leaf refusal: {error}") from error
    else:
        raise AssertionError("changed SetGeomScreen body was accepted")
    checks += 1

    missing_caller = bytearray(fixture)
    struct.pack_into("<I", missing_caller, address_offset(0x8003BD2C, 4), 0)
    try:
        measure(bytes(missing_caller))
    except MeasurementError as error:
        if "direct JAL callers" not in str(error):
            raise AssertionError(f"wrong missing-caller refusal: {error}") from error
    else:
        raise AssertionError("missing projection-producer caller was accepted")
    checks += 1

    added_ctc2 = bytearray(fixture)
    struct.pack_into(
        "<I", added_ctc2, address_offset(0x80020000, 4), 0x48C00000 | (24 << 11)
    )
    try:
        measure(bytes(added_ctc2))
    except MeasurementError as error:
        if "CTC2 CR24 census changed" not in str(error):
            raise AssertionError(f"wrong added-CTC2 refusal: {error}") from error
    else:
        raise AssertionError("added inline projection write was accepted")
    checks += 1
    print(f"measure_render_frontier selftest: {checks}/{checks} checks passed")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "executable",
        nargs="?",
        type=Path,
        default=ROOT / "scratch" / "raw" / "ctr" / CTR_USA.name,
        help="identity-verified extracted CTR USA PS-X EXE",
    )
    parser.add_argument(
        "--selftest", action="store_true", help="run asset-free both-answer fixtures"
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.selftest:
            selftest()
            return 0
        verify_executable(args.executable)
        frontier = measure(args.executable.read_bytes())
        print_report(frontier)
        return 0
    except (MeasurementError, ProvisionError, OSError) as error:
        print(f"measure_render_frontier: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
