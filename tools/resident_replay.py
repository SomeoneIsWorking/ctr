"""Build a bounded PS-X EXE register-state replay for a proven store-only prefix."""

from __future__ import annotations

import dataclasses
import pathlib
import struct


HEADER_SIZE = 0x800
ENTRY_OFFSET = 0x10
LOAD_OFFSET = 0x18
TEXT_SIZE_OFFSET = 0x1C
RAM_SIZE = 0x200000


class ReplayRefusal(RuntimeError):
    """The requested replay cannot preserve its stated evidence boundary."""


@dataclasses.dataclass(frozen=True)
class ReplayImage:
    data: bytes
    trampoline: int
    size: int


def _read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _load_immediate(register: int, value: int) -> tuple[int, int]:
    return (0x3C000000 | (register << 16) | (value >> 16),
            0x34000000 | (register << 21) | (register << 16) | (value & 0xFFFF))


def _validate_exe(data: bytes) -> tuple[int, int]:
    if len(data) < HEADER_SIZE or data[:8] != b"PS-X EXE":
        raise ReplayRefusal("input has no complete PS-X EXE header")
    load = _read_u32(data, LOAD_OFFSET)
    text_size = _read_u32(data, TEXT_SIZE_OFFSET)
    if text_size > len(data) - HEADER_SIZE:
        raise ReplayRefusal("PS-X EXE text size exceeds the available payload")
    if (load & (RAM_SIZE - 1)) + text_size > RAM_SIZE:
        raise ReplayRefusal("PS-X EXE text does not fit in main RAM")
    return load, text_size


def _physical_segments(addresses: range) -> tuple[range, ...]:
    start = addresses.start & (RAM_SIZE - 1)
    end = start + len(addresses)
    if end <= RAM_SIZE:
        return (range(start, end),)
    return (range(start, RAM_SIZE), range(0, end - RAM_SIZE))


def _overlaps_main_ram(left: range, right: range) -> bool:
    return any(
        left_part.start < right_part.stop and right_part.start < left_part.stop
        for left_part in _physical_segments(left)
        for right_part in _physical_segments(right)
    )


def _find_zero_run(payload: bytes, size: int, load: int, forbidden: tuple[range, ...]) -> int:
    zero_run = bytes(size)
    for offset in range(len(payload) - size, -1, -4):
        address = load + offset
        candidate = range(address, address + size)
        if any(_overlaps_main_ram(candidate, item) for item in forbidden):
            continue
        if payload[offset:offset + size] == zero_run:
            return offset
    raise ReplayRefusal(f"executable has no aligned {size}-byte zero run for the replay trampoline")


def build_replay(
    source: bytes,
    *,
    resume_target: int,
    registers: tuple[int, ...],
    lo: int,
    hi: int,
    expected_prefix: bytes,
    expected_ranges: tuple[tuple[int, bytes], ...] = (),
    expected_words: tuple[tuple[int, int], ...] = (),
    forbidden_ranges: tuple[range, ...] = (),
) -> ReplayImage:
    """Return a replay EXE, refusing unless the bounded original prefix is exact.

    ``registers`` is the complete 32-GPR state, including the immutable zero register.
    The replay is only sound through the checked code ranges and executable-backed words;
    callers must independently prove that they cover every instruction and memory read before
    the boundary they capture. ``expected_words`` covers executable-backed memory inputs,
    including zero image bytes that the real crt0 preserves as BSS state; it is not permission
    to assume arbitrary replay RAM equals post-crt0 RAM.
    """

    load, text_size = _validate_exe(source)
    if len(registers) != 32 or registers[0] != 0:
        raise ReplayRefusal("replay requires exactly 32 GPR values with r0 equal to zero")
    values = (*registers, lo, hi)
    if any(value < 0 or value > 0xFFFFFFFF for value in values):
        raise ReplayRefusal("replay register values must be unsigned 32-bit integers")
    if resume_target & 3:
        raise ReplayRefusal("resume target must be four-byte aligned")
    resume_offset = resume_target - load
    if resume_offset < 0 or resume_offset + len(expected_prefix) > text_size:
        raise ReplayRefusal("resume prefix lies outside executable text")
    payload = source[HEADER_SIZE:HEADER_SIZE + text_size]
    observed_prefix = payload[resume_offset:resume_offset + len(expected_prefix)]
    if observed_prefix != expected_prefix:
        raise ReplayRefusal("resident prefix bytes changed; the store-only replay proof is stale")
    for address, expected in expected_ranges:
        offset = address - load
        if offset < 0 or offset + len(expected) > text_size:
            raise ReplayRefusal("an expected continuation range lies outside executable text")
        if payload[offset:offset + len(expected)] != expected:
            raise ReplayRefusal(
                f"continuation bytes at 0x{address:08X} changed; the replay proof is stale"
            )
    for address, expected in expected_words:
        offset = address - load
        if offset < 0 or offset + 4 > text_size:
            raise ReplayRefusal("an expected executable-backed word lies outside executable text")
        if _read_u32(payload, offset) != expected:
            raise ReplayRefusal(
                f"executable-backed word at 0x{address:08X} changed; the replay proof is stale"
            )

    scratch = next((index for index in (26, 27, 25) if registers[index] == 0), None)
    if scratch is None:
        raise ReplayRefusal("replay needs a zero-valued k0, k1, or t9 scratch register")

    words: list[int] = []
    for index in range(1, 32):
        if index != scratch:
            words.extend(_load_immediate(index, registers[index]))
    words.extend(_load_immediate(scratch, lo))
    words.append(scratch << 21 | 0x13)  # mtlo scratch
    words.extend(_load_immediate(scratch, hi))
    words.append(scratch << 21 | 0x11)  # mthi scratch
    words.append(0)  # patched to j resume_target after placement
    words.append(scratch << 11 | 0x21)  # addu scratch,zero,zero (jump delay slot)
    trampoline_size = 4 * len(words)

    # The trampoline is part of the replay mechanism, not game state. It must never occupy any
    # byte later cited as an input to the proof. A service-state word at 0x8008D708 exposed this:
    # the former planner found a convenient BSS zero run at 0x8008D6F0, then its own `lui` word
    # became the value the replayed game loaded. Reserving only the resume prefix and stack spans
    # allowed the instrument to manufacture the divergence it reported.
    evidence_ranges = tuple(
        range(address, address + len(expected)) for address, expected in expected_ranges
    ) + tuple(range(address, address + 4) for address, _ in expected_words)
    prefix_range = range(resume_target, resume_target + len(expected_prefix))
    trampoline_offset = _find_zero_run(
        payload,
        trampoline_size,
        load,
        (prefix_range, *evidence_ranges, *forbidden_ranges),
    )
    trampoline = load + trampoline_offset
    jump_pc = trampoline + (len(words) - 2) * 4
    if ((jump_pc + 4) & 0xF0000000) != (resume_target & 0xF0000000):
        raise ReplayRefusal("trampoline and resume target are not reachable by one direct jump")
    words[-2] = 0x08000000 | ((resume_target >> 2) & 0x03FFFFFF)

    result = bytearray(source)
    struct.pack_into("<I", result, ENTRY_OFFSET, trampoline)
    struct.pack_into(f"<{len(words)}I", result, HEADER_SIZE + trampoline_offset, *words)
    return ReplayImage(bytes(result), trampoline, trampoline_size)


def write_replay(source: pathlib.Path, output: pathlib.Path, **arguments: object) -> ReplayImage:
    replay = build_replay(source.read_bytes(), **arguments)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(replay.data)
    return replay
