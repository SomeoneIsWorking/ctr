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
    leaf_model: int | None = None
    leaf_model_size: int = 0


@dataclasses.dataclass(frozen=True)
class ModeledMemset:
    """Explicit executable model for an external A(2Bh) memset leaf.

    The replay poisons the destination before entry, replaces the checked executable thunk with a
    jump to a register-preserving byte loop, and therefore cannot accidentally pass because fresh
    oracle RAM already happened to contain the requested fill byte.
    """

    thunk: int
    expected_thunk: bytes
    destination: int
    value: int
    size: int
    poison: int = 0xA5


@dataclasses.dataclass(frozen=True)
class ObservedZeroFill:
    """Instrument one real resident zero-fill without replacing its implementation.

    The original caller is redirected through a poison prelude, then back into the unchanged
    zero-fill body.  Its return is redirected through a checker which places the number of
    non-zero destination words in ``k0`` before reproducing the original next-call boundary.
    """

    callsite: int
    expected_callsite: bytes
    target: int
    expected_body: bytes
    return_pc: int
    expected_return: bytes
    destination: int
    words: int
    next_target: int
    next_ra: int
    next_a0: int
    poison: int = 0xA5


def _read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _load_immediate(register: int, value: int) -> tuple[int, int]:
    return (
        0x3C000000 | (register << 16) | (value >> 16),
        0x34000000 | (register << 21) | (register << 16) | (value & 0xFFFF),
    )


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
    return (range(start, RAM_SIZE), range(end - RAM_SIZE))


def _overlaps_main_ram(left: range, right: range) -> bool:
    return any(
        left_part.start < right_part.stop and right_part.start < left_part.stop
        for left_part in _physical_segments(left)
        for right_part in _physical_segments(right)
    )


def _find_zero_run(
    payload: bytes, size: int, load: int, forbidden: tuple[range, ...]
) -> int:
    zero_run = bytes(size)
    for offset in range(len(payload) - size, -1, -4):
        address = load + offset
        candidate = range(address, address + size)
        if any(_overlaps_main_ram(candidate, item) for item in forbidden):
            continue
        if payload[offset : offset + size] == zero_run:
            return offset
    raise ReplayRefusal(
        f"executable has no aligned {size}-byte zero run for the replay trampoline"
    )


def _modeled_memset_words(size: int) -> tuple[int, ...]:
    # Preserve every guest register except v0, which the BIOS leaf returns as the original a0.
    # Saving temporaries below guest sp would leave a RAM side effect that the generated-side leaf
    # model does not have. Instead, use the first twelve destination bytes as temporary storage,
    # fill the tail, restore v1/t0/t1, and finally overwrite the temporary bytes with a1.
    if size < 12:
        return (
            *(0xA0850000 | offset for offset in range(size)),  # sb a1,offset(a0)
            0x00801021,  # addu  v0,a0,zero
            0x03E00008,  # jr    ra
            0x00000000,  # nop
        )
    return (
        0xAC830000,  # sw    v1,0(a0)
        0xAC880004,  # sw    t0,4(a0)
        0xAC890008,  # sw    t1,8(a0)
        0x2488000C,  # addiu t0,a0,12
        0x24C9FFF4,  # addiu t1,a2,-12
        0x11200006,  # beq   t1,zero,restore
        0x00000000,  # nop
        0xA1050000,  # loop: sb a1,0(t0)
        0x25080001,  # addiu t0,t0,1
        0x2529FFFF,  # addiu t1,t1,-1
        0x1520FFFC,  # bne   t1,zero,loop
        0x00000000,  # nop
        0x8C890008,  # restore: lw t1,8(a0)
        0x8C880004,  # lw    t0,4(a0)
        0x8C830000,  # lw    v1,0(a0)
        *(0xA0850000 | offset for offset in range(12)),  # sb a1,offset(a0)
        0x00801021,  # addu  v0,a0,zero
        0x03E00008,  # jr    ra
        0x00000000,  # nop
    )


def _jump(address: int) -> int:
    return 0x08000000 | ((address >> 2) & 0x03FFFFFF)


def _jump_and_link(address: int) -> int:
    return 0x0C000000 | ((address >> 2) & 0x03FFFFFF)


def _zero_fill_poison_words(model: ObservedZeroFill) -> tuple[int, ...]:
    poison_word = model.poison * 0x01010101
    return (
        *_load_immediate(2, model.destination),  # v0 = destination
        0x24030000 | model.words,  # addiu v1,zero,words
        *_load_immediate(25, poison_word),  # t9 = repeated poison byte
        0xAC590000,  # loop: sw t9,0(v0)
        0x24420004,  # addiu v0,v0,4
        0x2463FFFF,  # addiu v1,v1,-1
        0x1460FFFC,  # bne v1,zero,loop
        0x00000000,  # nop
        0x0000C821,  # addu t9,zero,zero
        _jump(model.target),
        0x00000000,  # nop
    )


def _zero_fill_check_words(model: ObservedZeroFill) -> tuple[int, ...]:
    return (
        *_load_immediate(26, model.destination),  # k0 = destination
        0x241B0000 | model.words,  # addiu k1,zero,words
        0x0000C821,  # addu t9,zero,zero
        0x8F430000,  # loop: lw v1,0(k0)
        0x00000000,  # nop (PSX load delay)
        0x0003182B,  # sltu v1,zero,v1
        0x0323C821,  # addu t9,t9,v1
        0x275A0004,  # addiu k0,k0,4
        0x277BFFFF,  # addiu k1,k1,-1
        0x1760FFF9,  # bne k1,zero,loop
        0x00000000,  # nop
        0x0320D021,  # addu k0,t9,zero (observable non-zero-word count)
        0x0000D821,  # addu k1,zero,zero
        0x0000C821,  # addu t9,zero,zero
        0x2403FFFF,  # addiu v1,zero,-1 (real zero-fill postcondition)
        *_load_immediate(31, model.next_ra),
        _jump(model.next_target),
        0x26040000 | (model.next_a0 & 0xFFFF),  # addiu a0,s0,next_a0
    )


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
    modeled_memset: ModeledMemset | None = None,
    observed_zero_fill: ObservedZeroFill | None = None,
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
        raise ReplayRefusal(
            "replay requires exactly 32 GPR values with r0 equal to zero"
        )
    values = (*registers, lo, hi)
    if any(value < 0 or value > 0xFFFFFFFF for value in values):
        raise ReplayRefusal("replay register values must be unsigned 32-bit integers")
    if resume_target & 3:
        raise ReplayRefusal("resume target must be four-byte aligned")
    resume_offset = resume_target - load
    if resume_offset < 0 or resume_offset + len(expected_prefix) > text_size:
        raise ReplayRefusal("resume prefix lies outside executable text")
    payload = source[HEADER_SIZE : HEADER_SIZE + text_size]
    observed_prefix = payload[resume_offset : resume_offset + len(expected_prefix)]
    if observed_prefix != expected_prefix:
        raise ReplayRefusal(
            "resident prefix bytes changed; the store-only replay proof is stale"
        )
    for address, expected in expected_ranges:
        offset = address - load
        if offset < 0 or offset + len(expected) > text_size:
            raise ReplayRefusal(
                "an expected continuation range lies outside executable text"
            )
        if payload[offset : offset + len(expected)] != expected:
            raise ReplayRefusal(
                f"continuation bytes at 0x{address:08X} changed; the replay proof is stale"
            )
    for address, expected in expected_words:
        offset = address - load
        if offset < 0 or offset + 4 > text_size:
            raise ReplayRefusal(
                "an expected executable-backed word lies outside executable text"
            )
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
    model_words: tuple[int, ...] = ()
    model_size = 0
    model_offset: int | None = None
    model_address: int | None = None
    model_ranges: tuple[range, ...] = ()
    if modeled_memset is not None:
        model = modeled_memset
        if not model.expected_thunk or len(model.expected_thunk) != 12:
            raise ReplayRefusal(
                "modeled memset requires the complete 12-byte executable thunk"
            )
        if model.thunk & 3 or model.destination & 3:
            raise ReplayRefusal(
                "modeled memset thunk and destination must be four-byte aligned"
            )
        if not 0 <= model.value <= 0xFF or not 0 <= model.poison <= 0xFF:
            raise ReplayRefusal("modeled memset value and poison must be bytes")
        if model.value == model.poison:
            raise ReplayRefusal(
                "modeled memset poison must differ from the requested fill byte"
            )
        if model.size <= 0:
            raise ReplayRefusal("modeled memset requires a nonempty destination")
        thunk_offset = model.thunk - load
        if thunk_offset < 0 or thunk_offset + len(model.expected_thunk) > text_size:
            raise ReplayRefusal(
                "modeled memset thunk lies outside original executable text"
            )
        if (
            payload[thunk_offset : thunk_offset + len(model.expected_thunk)]
            != model.expected_thunk
        ):
            raise ReplayRefusal(
                "modeled memset thunk bytes changed; the external-leaf proof is stale"
            )
        destination_end = model.destination + model.size
        if destination_end > 0x1_0000_0000 or model.destination < load + text_size:
            raise ReplayRefusal(
                "modeled memset poison must occupy BSS beyond original executable text"
            )
        if (model.destination & (RAM_SIZE - 1)) + model.size > RAM_SIZE:
            raise ReplayRefusal("modeled memset destination wraps PSX main RAM")
        model_words = _modeled_memset_words(model.size)
        model_size = 4 * len(model_words)
        model_offset = _find_zero_run(
            payload,
            model_size,
            load,
            (
                range(model.thunk, model.thunk + len(model.expected_thunk)),
                range(model.destination, destination_end),
                prefix_range,
                *evidence_ranges,
                *forbidden_ranges,
            ),
        )
        model_address = load + model_offset
        model_ranges = (range(model_address, model_address + model_size),)

    zero_fill_ranges: tuple[range, ...] = ()
    zero_fill_poison_offset: int | None = None
    zero_fill_check_offset: int | None = None
    zero_fill_poison_address: int | None = None
    zero_fill_check_address: int | None = None
    zero_fill_poison_code: tuple[int, ...] = ()
    zero_fill_check_code: tuple[int, ...] = ()
    if observed_zero_fill is not None:
        observed = observed_zero_fill
        if (
            observed.callsite & 3
            or observed.target & 3
            or observed.return_pc & 3
            or observed.destination & 3
            or observed.next_target & 3
            or observed.next_ra & 3
        ):
            raise ReplayRefusal(
                "observed zero-fill addresses must be four-byte aligned"
            )
        if observed.words <= 0 or observed.words > 0x7FFF:
            raise ReplayRefusal(
                "observed zero-fill word count must fit a positive addiu immediate"
            )
        if not 0 <= observed.next_a0 <= 0x7FFF:
            raise ReplayRefusal("observed zero-fill next-call a0 offset must fit addiu")
        if not 0 <= observed.poison <= 0xFF or observed.poison == 0:
            raise ReplayRefusal("observed zero-fill poison must be a non-zero byte")
        if len(observed.expected_callsite) != 8 or len(observed.expected_return) != 8:
            raise ReplayRefusal(
                "observed zero-fill requires exact two-instruction caller slices"
            )
        if not observed.expected_body or len(observed.expected_body) % 4:
            raise ReplayRefusal(
                "observed zero-fill requires a complete aligned function body"
            )
        checked_slices = (
            (observed.callsite, observed.expected_callsite, "callsite"),
            (observed.target, observed.expected_body, "body"),
            (observed.return_pc, observed.expected_return, "return continuation"),
        )
        for address, expected, label in checked_slices:
            offset = address - load
            if offset < 0 or offset + len(expected) > text_size:
                raise ReplayRefusal(
                    f"observed zero-fill {label} lies outside executable text"
                )
            if payload[offset : offset + len(expected)] != expected:
                raise ReplayRefusal(f"observed zero-fill {label} bytes changed")
        destination_size = observed.words * 4
        destination_offset = observed.destination - load
        if destination_offset < 0 or destination_offset + destination_size > text_size:
            raise ReplayRefusal(
                "observed zero-fill destination lies outside executable text"
            )
        if any(payload[destination_offset : destination_offset + destination_size]):
            raise ReplayRefusal(
                "observed zero-fill destination is not initially all zero"
            )

        zero_fill_poison_code = _zero_fill_poison_words(observed)
        zero_fill_check_code = _zero_fill_check_words(observed)
        poison_size = 4 * len(zero_fill_poison_code)
        check_size = 4 * len(zero_fill_check_code)
        occupied = (
            prefix_range,
            *evidence_ranges,
            *model_ranges,
            *forbidden_ranges,
            range(observed.callsite, observed.callsite + 8),
            range(observed.target, observed.target + len(observed.expected_body)),
            range(observed.return_pc, observed.return_pc + 8),
            range(observed.destination, observed.destination + destination_size),
        )
        zero_fill_poison_offset = _find_zero_run(payload, poison_size, load, occupied)
        zero_fill_poison_address = load + zero_fill_poison_offset
        poison_range = range(
            zero_fill_poison_address, zero_fill_poison_address + poison_size
        )
        zero_fill_check_offset = _find_zero_run(
            payload, check_size, load, (*occupied, poison_range)
        )
        zero_fill_check_address = load + zero_fill_check_offset
        zero_fill_ranges = (
            poison_range,
            range(zero_fill_check_address, zero_fill_check_address + check_size),
        )

    trampoline_offset = _find_zero_run(
        payload,
        trampoline_size,
        load,
        (
            prefix_range,
            *evidence_ranges,
            *model_ranges,
            *zero_fill_ranges,
            *forbidden_ranges,
        ),
    )
    trampoline = load + trampoline_offset
    jump_pc = trampoline + (len(words) - 2) * 4
    if ((jump_pc + 4) & 0xF0000000) != (resume_target & 0xF0000000):
        raise ReplayRefusal(
            "trampoline and resume target are not reachable by one direct jump"
        )
    words[-2] = 0x08000000 | ((resume_target >> 2) & 0x03FFFFFF)

    result = bytearray(source)
    if modeled_memset is not None:
        assert model_offset is not None and model_address is not None
        destination_end = modeled_memset.destination + modeled_memset.size
        extended_text_size = (destination_end - load + 0x7FF) & ~0x7FF
        required_file_size = HEADER_SIZE + extended_text_size
        if len(result) < required_file_size:
            result.extend(bytes(required_file_size - len(result)))
        struct.pack_into("<I", result, TEXT_SIZE_OFFSET, extended_text_size)
        destination_offset = HEADER_SIZE + modeled_memset.destination - load
        result[destination_offset : destination_offset + modeled_memset.size] = (
            bytes([modeled_memset.poison]) * modeled_memset.size
        )
        struct.pack_into(
            f"<{len(model_words)}I", result, HEADER_SIZE + model_offset, *model_words
        )
        thunk_offset = HEADER_SIZE + modeled_memset.thunk - load
        jump = 0x08000000 | ((model_address >> 2) & 0x03FFFFFF)
        struct.pack_into("<III", result, thunk_offset, jump, 0, 0)
    if observed_zero_fill is not None:
        assert (
            zero_fill_poison_offset is not None and zero_fill_check_offset is not None
        )
        assert (
            zero_fill_poison_address is not None and zero_fill_check_address is not None
        )
        struct.pack_into(
            f"<{len(zero_fill_poison_code)}I",
            result,
            HEADER_SIZE + zero_fill_poison_offset,
            *zero_fill_poison_code,
        )
        struct.pack_into(
            f"<{len(zero_fill_check_code)}I",
            result,
            HEADER_SIZE + zero_fill_check_offset,
            *zero_fill_check_code,
        )
        callsite_offset = HEADER_SIZE + observed_zero_fill.callsite - load
        struct.pack_into(
            "<I", result, callsite_offset, _jump_and_link(zero_fill_poison_address)
        )
        return_offset = HEADER_SIZE + observed_zero_fill.return_pc - load
        struct.pack_into(
            "<II", result, return_offset, _jump(zero_fill_check_address), 0
        )
    struct.pack_into("<I", result, ENTRY_OFFSET, trampoline)
    struct.pack_into(f"<{len(words)}I", result, HEADER_SIZE + trampoline_offset, *words)
    return ReplayImage(
        bytes(result), trampoline, trampoline_size, model_address, model_size
    )


def write_replay(
    source: pathlib.Path, output: pathlib.Path, **arguments: object
) -> ReplayImage:
    replay = build_replay(source.read_bytes(), **arguments)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(replay.data)
    return replay
