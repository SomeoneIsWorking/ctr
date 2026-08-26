#!/usr/bin/env python3
"""Compare CTR's independent oracle boundaries with shipping generated execution."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import struct
import subprocess
import sys

from resident_replay import (
    ModeledMemset,
    ObservedZeroFill,
    ReplayRefusal,
    write_replay,
)

REGISTER_NAMES = (
    "at",
    "v0",
    "v1",
    "a0",
    "a1",
    "a2",
    "a3",
    "t0",
    "t1",
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
    "t7",
    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "t8",
    "t9",
    "k0",
    "k1",
    "gp",
    "sp",
    "fp",
    "ra",
    "lo",
    "hi",
)


def pack_words(words: tuple[int, ...]) -> bytes:
    return struct.pack(f"<{len(words)}I", *words)


RESIDENT_RESUME_TARGET = 0x8003C58C
RESIDENT_PREFIX_WORDS = (
    0x27BDFFC0,
    0xAFBF0038,
    0xAFB50034,
    0xAFB40030,
    0xAFB3002C,
    0xAFB20028,
    0xAFB10024,
    0x0C01DE79,
    0xAFB00020,
)
RESIDENT_PREFIX = pack_words(RESIDENT_PREFIX_WORDS)

RUNTIME_INIT_TARGET = 0x800779E4
RUNTIME_INIT_WORDS = (
    0x3C088009,
    0x8D08C050,
    0x27BDFFF0,
    0xAFB00004,
    0xAFB10008,
    0xAFBF000C,
    0x1500000F,
    0x34080001,
    0x3C018009,
    0xAC28C050,
    0x3C108009,
    0x2610D668,
    0x3C110000,
    0x26310000,
    0x12200007,
    0x00000000,
    0x8E080000,
    0x26100004,
    0x0100F809,
    0x2631FFFF,
    0x1620FFFB,
    0x00000000,
    0x8FBF000C,
    0x8FB10008,
    0x8FB00004,
    0x27BD0010,
    0x03E00008,
    0x00000000,
)
RUNTIME_INIT_CODE = pack_words(RUNTIME_INIT_WORDS)
POST_RUNTIME_INIT_TARGET = 0x8003C5B0
POST_RUNTIME_INIT_WORDS = (
    0x8F830188,
    0x24020005,
    0x10620264,
    0x241300D8,
    0x24140001,
    0x2412FFFF,
    0x24110001,
    0x2415FFFA,
    0x0C00CB70,
    0x00000000,
)
POST_RUNTIME_INIT_CODE = pack_words(POST_RUNTIME_INIT_WORDS)
RUNTIME_INIT_NEXT_CALL = 0x80032DC0
RUNTIME_INIT_FLAG = 0x8008C050
INITIAL_MODE_WORD = 0x8008D0F4
BSS_START = 0x8008D668

# The startup service takes this exact idle path for the original post-crt0 state. Ghidra's
# decompile establishes the four reads and the control flow; the bounded replay checks both the
# executed instruction islands and their source-image inputs before treating the next call as
# evidence. Keeping the islands separate avoids claiming unexecuted branches in the same function.
STARTUP_SERVICE_ENTRY_WORDS = (
    0x93820134,
    0x27BDFFE0,
    0xAFBF001C,
    0x1040006A,
    0xAFB00018,
    0x3C028009,
    0x8C42D708,
    0x00000000,
    0x14400065,
    0x00000000,
    0x87820136,
    0x00000000,
    0x10400061,
    0x00000000,
)
STARTUP_SERVICE_IDLE_WORDS = (0x8F83013C, 0x00000000, 0x1060001A, 0x00000000)
STARTUP_SERVICE_RETURN_WORDS = (0x8FBF001C, 0x8FB00018, 0x03E00008, 0x27BD0020)
POST_STARTUP_SERVICE_WORDS = (0x0C00741B, 0x00000000)
STARTUP_SERVICE_ENTRY = 0x80032DC0
STARTUP_SERVICE_IDLE = 0x80032F78
STARTUP_SERVICE_RETURN = 0x80032FEC
POST_STARTUP_SERVICE = 0x8003C5D8
STARTUP_SERVICE_NEXT_CALL = 0x8001D06C
STARTUP_SERVICE_REQUEST_WORD = 0x8008D0A0
STARTUP_SERVICE_LOADING_FLAG = 0x8008D708
STARTUP_SERVICE_TIMESTAMP = 0x8008D0A8
STARTUP_SERVICE_ENTRY_CODE = pack_words(STARTUP_SERVICE_ENTRY_WORDS)
STARTUP_SERVICE_IDLE_CODE = pack_words(STARTUP_SERVICE_IDLE_WORDS)
STARTUP_SERVICE_RETURN_CODE = pack_words(STARTUP_SERVICE_RETURN_WORDS)
POST_STARTUP_SERVICE_CODE = pack_words(POST_STARTUP_SERVICE_WORDS)

# Startup state zero makes the next polling service return without calling its optional worker.
# The caller then dispatches through the first executable-backed jump-table entry and calls the
# A(2Bh) memset thunk. The proof stops at that thunk: its jump to 0xA0 is an external BIOS leaf with
# a RAM side effect, not executable code that this bounded replay may silently step through.
BACKGROUND_SERVICE_ENTRY_WORDS = (0x8F82074C, 0x27BDFFE8, 0x10400003, 0xAFBF0010)
BACKGROUND_SERVICE_RETURN_WORDS = (0x8FBF0010, 0x00000000, 0x03E00008, 0x27BD0018)
STATE_DISPATCH_WORDS = (
    0x8F830188,
    0x00000000,
    0x2C620005,
    0x10400255,
    0x24020005,
    0x3C028001,
    0x24421594,
    0x00031880,
    0x00621821,
    0x8C620000,
    0x00000000,
    0x00400008,
    0x00000000,
)
STATE_ZERO_CASE_WORDS = (0x00002821, 0x8F840340, 0x0C01C62F, 0x24062584)
BACKGROUND_SERVICE = 0x8001D06C
BACKGROUND_SERVICE_RETURN = 0x8001D084
POST_BACKGROUND_SERVICE = 0x8003C5E0
STATE_ZERO_CASE = 0x8003C614
STARTUP_MEMSET_THUNK = 0x800718BC
STARTUP_MEMSET_THUNK_WORDS = (0x240A00A0, 0x01400008, 0x2409002B)
POST_STARTUP_MEMSET_WORDS = (0x0C01DF36, 0x00002021)
POST_STARTUP_MEMSET = 0x8003C624
STARTUP_MEMSET_NEXT_CALL = 0x80077CD8

# After the modeled A(2Bh) leaf returns, the state-zero case calls the executable swap 0x80077CD8:
# it stores a0 into initialized word 0x8008C0B4 in its delay slot and returns the previous value.
# The caller then calls 0x800771C4, which loads table pointer 0x8008C000 from initialized word
# 0x8008C020 and jumps indirectly through slot +0xC to 0x800772E0, the one-time system initializer
# (Ghidra: FUN_800772e0 guards all of its work on word 0x8008AF98). Two windows cover this: the
# init-swap window stops at 0x800771C4's entry; the init-dispatch window additionally checks the
# dispatcher's executed prefix plus both dispatch words and stops at the indirect target's entry.
INIT_DISPATCH_TABLE_BASE = 0x8008C000
STARTUP_INIT_SWAP_WORDS = (0x3C028009, 0x8C42C0B4, 0x3C018009, 0x03E00008, 0xAC24C0B4)
POST_INIT_SWAP_WORDS = (0x0C01DC71, 0x00000000)
INIT_DISPATCHER_PREFIX_WORDS = (
    0x3C028009,
    0x8C42C020,
    0x27BDFFE8,
    0xAFBF0010,
    0x8C42000C,
    0x00000000,
    0x0040F809,
    0x00000000,
)
STARTUP_INIT_SWAP = 0x80077CD8
POST_INIT_SWAP = 0x8003C62C
INIT_DISPATCHER = 0x800771C4
INIT_NEXT_CALL = 0x800772E0
INITIALIZER_DEVICE_NEXT_CALL = 0x800777E8
INITIALIZER_DEVICE_PREFIX_WORDS = (
    0x27BDFFE8,
    0xAFB00010,
    0x3C108009,
    0x2610AF98,
    0xAFBF0014,
    0x96020000,
    0x00000000,
    0x1440002A,
    0x00001021,
    0x3C038009,
    0x8C63C024,
    0x3C028009,
    0x8C42C028,
    0x3C053333,
    0xA4400000,
    0x94420000,
    0x34A53333,
    0xA4620000,
    0x3C028009,
    0x8C42C02C,
    0x02002021,
    0xAC450000,
    0x0C01DDFA,
    0x2405041A,
)
INITIALIZER_DEVICE_PREFIX = pack_words(INITIALIZER_DEVICE_PREFIX_WORDS)
ZERO_FILL_CALLSITE = 0x80077338
ZERO_FILL_TARGET = 0x800777E8
ZERO_FILL_RETURN = 0x80077340
ZERO_FILL_NEXT_CALL = 0x80080260
ZERO_FILL_DESTINATION = 0x8008AF98
ZERO_FILL_WORDS = 0x41A
ZERO_FILL_POISON = 0xA5
ZERO_FILL_CALLSITE_CODE = INITIALIZER_DEVICE_PREFIX[-8:]
ZERO_FILL_BODY = pack_words(
    (
        0x10A00006,
        0x24A2FFFF,
        0x2403FFFF,
        0xAC800000,
        0x2442FFFF,
        0x1443FFFD,
        0x24840004,
        0x03E00008,
        0x00000000,
    )
)
ZERO_FILL_RETURN_CODE = pack_words((0x0C020098, 0x26040038))
INIT_SWAP_DATA_WORD = 0x8008C0B4
INIT_DISPATCH_TABLE = 0x8008C020
INIT_DISPATCH_SLOT = INIT_DISPATCH_TABLE_BASE + 0xC
BACKGROUND_SERVICE_PENDING = 0x8008D6B8
STATE_JUMP_TABLE_ZERO = 0x80011594
GLOBAL_STATE_POINTER = 0x8008D2AC
GLOBAL_STATE_BASE = 0x80096B20
BACKGROUND_SERVICE_ENTRY_CODE = pack_words(BACKGROUND_SERVICE_ENTRY_WORDS)
BACKGROUND_SERVICE_RETURN_CODE = pack_words(BACKGROUND_SERVICE_RETURN_WORDS)
STATE_DISPATCH_CODE = pack_words(STATE_DISPATCH_WORDS)
STATE_ZERO_CASE_CODE = pack_words(STATE_ZERO_CASE_WORDS)
STARTUP_MEMSET_THUNK_CODE = pack_words(STARTUP_MEMSET_THUNK_WORDS)
POST_STARTUP_MEMSET_CODE = pack_words(POST_STARTUP_MEMSET_WORDS)
STARTUP_INIT_SWAP_CODE = pack_words(STARTUP_INIT_SWAP_WORDS)
POST_INIT_SWAP_CODE = pack_words(POST_INIT_SWAP_WORDS)
INIT_DISPATCHER_CODE = pack_words(INIT_DISPATCHER_PREFIX_WORDS)


class Refusal(RuntimeError):
    """The evidence was incomplete, so no agreement can be claimed."""


@dataclasses.dataclass(frozen=True)
class Boundary:
    target: int
    pc: int
    registers: dict[str, int]
    step: int | None = None


@dataclasses.dataclass(frozen=True)
class DeviceBoundary:
    pc: int
    values: dict[str, int]


@dataclasses.dataclass(frozen=True)
class MemoryBoundary:
    pc: int
    destination: int
    size: int
    poison: int
    nonzero_words: int


@dataclasses.dataclass(frozen=True)
class ModeledReturn:
    table: str
    function: int
    target: int
    return_pc: int
    v0: int
    v1: int
    boundary: Boundary
    step: int | None = None


@dataclasses.dataclass(frozen=True)
class PostInitHeapEvidence:
    first_call: Boundary
    modeled_return: ModeledReturn
    post_return_call: Boundary


def run(command: list[str], label: str) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, check=False, timeout=60
        )
    except subprocess.TimeoutExpired as error:
        raise Refusal(f"{label} exceeded the 60-second evidence window") from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise Refusal(
            f"{label} refused or failed with exit {result.returncode}:\n{detail}"
        )
    return result


def parse_register_block(
    text: str, tag: str, label: str
) -> tuple[int, dict[str, int], int | None]:
    header = re.search(
        rf"^# {re.escape(tag)}-REGS(?: step=(\d+))? pc=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    register_pattern = re.compile(
        rf"^# {re.escape(tag)}-REG (\w+)=0x([0-9A-Fa-f]+)\s*$", re.MULTILINE
    )
    if header is None:
        raise Refusal(f"{label} has no complete {tag} register block")
    registers = {name: int(value, 16) for name, value in register_pattern.findall(text)}
    missing = sorted(set(REGISTER_NAMES) - registers.keys())
    extra = sorted(registers.keys() - set(REGISTER_NAMES))
    if missing or extra:
        raise Refusal(
            f"{label} register coverage changed (missing={missing or 'none'}, extra={extra or 'none'})"
        )
    step = int(header.group(1)) if header.group(1) is not None else None
    return int(header.group(2), 16), registers, step


def parse_call_boundary(
    text: str, capture_tag: str, register_tag: str, label: str
) -> Boundary:
    capture = re.search(
        rf"^# {re.escape(capture_tag)} target=0x([0-9A-Fa-f]+) "
        rf"ra=0x([0-9A-Fa-f]+)(?: step=(\d+))?\s*$",
        text,
        re.MULTILINE,
    )
    if capture is None:
        raise Refusal(f"{label} has no {capture_tag} record")
    pc, registers, register_step = parse_register_block(text, register_tag, label)
    captured_ra = int(capture.group(2), 16)
    if captured_ra != registers["ra"]:
        raise Refusal(
            f"{label} capture $ra 0x{captured_ra:08X} disagrees with "
            f"register block 0x{registers['ra']:08X}"
        )
    capture_step = int(capture.group(3)) if capture.group(3) is not None else None
    if (
        capture_step is not None
        and register_step is not None
        and capture_step != register_step
    ):
        raise Refusal(
            f"{label} capture step {capture_step} disagrees with register step {register_step}"
        )
    return Boundary(
        int(capture.group(1), 16), pc, registers, capture_step or register_step
    )


def parse_boundary(text: str, port: bool) -> Boundary:
    prefix = "PORT-" if port else ""
    return parse_call_boundary(
        text,
        f"{prefix}CAPTURED-CALL",
        f"{prefix}CALL-BOUNDARY",
        "port" if port else "oracle",
    )


def parse_pc_boundary(text: str, label: str) -> Boundary:
    capture = re.search(
        r"^# CAPTURED-PC target=0x([0-9A-Fa-f]+) executed=(\d+)\s*$",
        text,
        re.MULTILINE,
    )
    header = re.search(
        r"^# PC-BOUNDARY-REGS executed=(\d+) pc=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    registers = {
        name: int(value, 16)
        for name, value in re.findall(
            r"^# PC-BOUNDARY-REG (\w+)=0x([0-9A-Fa-f]+)\s*$",
            text,
            re.MULTILINE,
        )
    }
    if capture is None or header is None:
        raise Refusal(f"{label} has no complete PC boundary")
    missing = sorted(set(REGISTER_NAMES) - registers.keys())
    extra = sorted(registers.keys() - set(REGISTER_NAMES))
    if missing or extra:
        raise Refusal(
            f"{label} register coverage changed (missing={missing or 'none'}, extra={extra or 'none'})"
        )
    captured_target = int(capture.group(1), 16)
    captured_executed = int(capture.group(2))
    block_executed = int(header.group(1))
    block_pc = int(header.group(2), 16)
    if captured_target != block_pc or captured_executed != block_executed:
        raise Refusal(f"{label} PC metadata and register block disagree")
    return Boundary(captured_target, block_pc, registers, captured_executed)


def parse_device_boundary(text: str, port: bool, expected_pc: int) -> DeviceBoundary:
    prefix = "PORT-" if port else ""
    label = "port" if port else "oracle"
    headers = re.findall(
        rf"^# {prefix}DEVICE-BOUNDARY schema=(\d+) pc=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    rows = re.findall(
        rf"^# {prefix}DEVICE-REG (\w+) value=0x([0-9A-Fa-f]+) "
        rf"mask=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    if len(headers) != 1 or headers[0][0] != "1":
        raise Refusal(f"{label} has no unique device schema 1 boundary")
    pc = int(headers[0][1], 16)
    if pc != expected_pc:
        raise Refusal(
            f"{label} device PC 0x{pc:08X} disagrees with CPU boundary 0x{expected_pc:08X}"
        )
    expected_masks = {"I_STAT": 0x7FF, "I_MASK": 0x7FF, "DPCR": 0xFFFFFFFF}
    values: dict[str, int] = {}
    for name, raw_value, raw_mask in rows:
        if name not in expected_masks or name in values:
            raise Refusal(
                f"{label} device register coverage is duplicated or unknown: {name}"
            )
        mask = int(raw_mask, 16)
        if mask != expected_masks[name]:
            raise Refusal(
                f"{label} {name} mask is 0x{mask:08X}, expected 0x{expected_masks[name]:08X}"
            )
        value = int(raw_value, 16)
        if value & ~mask:
            raise Refusal(
                f"{label} {name} value 0x{value:08X} exceeds its declared mask"
            )
        values[name] = value
    if set(values) != set(expected_masks):
        raise Refusal(
            f"{label} device coverage changed: got {sorted(values)}, expected {sorted(expected_masks)}"
        )
    return DeviceBoundary(pc, values)


def parse_memory_boundary(text: str, expected_pc: int) -> MemoryBoundary:
    headers = re.findall(
        r"^# PORT-MEMORY-BOUNDARY schema=(\d+) pc=0x([0-9A-Fa-f]+) "
        r"destination=0x([0-9A-Fa-f]+) size=0x([0-9A-Fa-f]+) "
        r"poison=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    results = re.findall(
        r"^# PORT-MEMORY-RESULT nonzero_words=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    if len(headers) != 1 or headers[0][0] != "1" or len(results) != 1:
        raise Refusal("port has no unique memory schema 1 boundary")
    pc = int(headers[0][1], 16)
    if pc != expected_pc:
        raise Refusal(
            f"port memory PC 0x{pc:08X} disagrees with CPU boundary 0x{expected_pc:08X}"
        )
    boundary = MemoryBoundary(
        pc,
        int(headers[0][2], 16),
        int(headers[0][3], 16),
        int(headers[0][4], 16),
        int(results[0], 16),
    )
    if (
        boundary.destination != ZERO_FILL_DESTINATION
        or boundary.size != ZERO_FILL_WORDS * 4
        or boundary.poison != ZERO_FILL_POISON
    ):
        raise Refusal("port memory boundary changed its zero-fill range or poison")
    if boundary.nonzero_words > ZERO_FILL_WORDS:
        raise Refusal(
            "port memory boundary reports more non-zero words than it examined"
        )
    return boundary


def parse_modeled_return(text: str, port: bool) -> ModeledReturn:
    prefix = "PORT-" if port else ""
    match = re.search(
        rf"^# {prefix}MODELED-BIOS-RETURN table=([ABC]) function=0x([0-9A-Fa-f]+) "
        rf"target=0x([0-9A-Fa-f]+) ra=0x([0-9A-Fa-f]+) v0=0x([0-9A-Fa-f]+) "
        rf"v1=0x([0-9A-Fa-f]+)(?: step=(\d+))?\s*$",
        text,
        re.MULTILINE,
    )
    label = "port modeled return" if port else "oracle modeled return"
    if match is None:
        raise Refusal(f"{label} metadata is missing")
    pc, registers, block_step = parse_register_block(
        text, f"{prefix}MODELED-RETURN", label
    )
    metadata_step = int(match.group(7)) if match.group(7) is not None else None
    if (
        metadata_step is not None
        and block_step is not None
        and metadata_step != block_step
    ):
        raise Refusal(f"{label} metadata and register steps disagree")
    return_pc = int(match.group(4), 16)
    return ModeledReturn(
        match.group(1),
        int(match.group(2), 16),
        int(match.group(3), 16),
        return_pc,
        int(match.group(5), 16),
        int(match.group(6), 16),
        Boundary(return_pc, pc, registers, metadata_step or block_step),
        metadata_step or block_step,
    )


def parse_post_init_heap(text: str, port: bool) -> PostInitHeapEvidence:
    prefix = "PORT-" if port else ""
    return PostInitHeapEvidence(
        parse_boundary(text, port),
        parse_modeled_return(text, port),
        parse_call_boundary(
            text,
            f"{prefix}POST-RETURN-CAPTURED-CALL",
            f"{prefix}POST-RETURN-CALL-BOUNDARY",
            "port post-return call" if port else "oracle post-return call",
        ),
    )


def compare_boundary(
    oracle: Boundary, port: Boundary, stage: str
) -> list[tuple[str, int, int]]:
    if oracle.target != oracle.pc:
        raise Refusal(
            f"oracle {stage} captured target 0x{oracle.target:08X} but boundary PC is 0x{oracle.pc:08X}"
        )
    if port.target != oracle.target:
        raise Refusal(
            f"port {stage} claims target 0x{port.target:08X}, not oracle target 0x{oracle.target:08X}"
        )
    rows = [(f"{stage}.pc", oracle.pc, port.pc)]
    rows.extend(
        (f"{stage}.{name}", oracle.registers[name], port.registers[name])
        for name in REGISTER_NAMES
    )
    return rows


def compare_devices(
    oracle: DeviceBoundary, port: DeviceBoundary
) -> list[tuple[str, int, int]]:
    if port.pc != oracle.pc:
        raise Refusal(
            f"port device PC 0x{port.pc:08X} does not match oracle device PC 0x{oracle.pc:08X}"
        )
    return [
        (f"device.{name}", oracle.values[name], port.values[name])
        for name in ("I_STAT", "I_MASK", "DPCR")
    ]


def compare_memory(
    oracle: MemoryBoundary, port: MemoryBoundary
) -> list[tuple[str, int, int]]:
    if port.pc != oracle.pc:
        raise Refusal(
            f"port memory PC 0x{port.pc:08X} does not match oracle memory PC 0x{oracle.pc:08X}"
        )
    metadata = (
        ("memory.destination", oracle.destination, port.destination),
        ("memory.size", oracle.size, port.size),
        ("memory.poison", oracle.poison, port.poison),
        ("memory.nonzero_words", oracle.nonzero_words, port.nonzero_words),
    )
    return list(metadata)


def compare(oracle: Boundary, port: Boundary) -> list[tuple[str, int, int]]:
    return [
        (name.removeprefix("first."), left, right)
        for name, left, right in compare_boundary(oracle, port, "first")
    ]


def compare_post_init_heap(
    oracle: PostInitHeapEvidence, port: PostInitHeapEvidence
) -> list[tuple[str, int, int]]:
    rows = compare_boundary(oracle.first_call, port.first_call, "first")
    oracle_model = oracle.modeled_return
    port_model = port.modeled_return
    rows.extend(
        (
            ("model.table", ord(oracle_model.table), ord(port_model.table)),
            ("model.function", oracle_model.function, port_model.function),
            ("model.target", oracle_model.target, port_model.target),
            ("model.ra", oracle_model.return_pc, port_model.return_pc),
            ("model.v0", oracle_model.v0, port_model.v0),
            ("model.v1", oracle_model.v1, port_model.v1),
        )
    )
    rows.extend(compare_boundary(oracle_model.boundary, port_model.boundary, "modeled"))
    rows.extend(
        compare_boundary(oracle.post_return_call, port.post_return_call, "post")
    )
    return rows


def print_rows(rows: list[tuple[str, int, int]], label: str) -> int:
    differences = 0
    print("  field              oracle       generated     verdict")
    print("  " + "-" * 60)
    for name, oracle, port in rows:
        verdict = "AGREE" if oracle == port else "DISAGREE"
        differences += oracle != port
        print(f"  {name:<15}  0x{oracle:08X}   0x{port:08X}   {verdict}")
    print(
        f"{label}: {len(rows) - differences}/{len(rows)} fields agree; {differences} differ"
    )
    return differences


def selftest() -> int:
    from compare_crt0_trace_selftest import selftest as run_selftest

    return run_selftest()


def parse_forced_field(text: str, default_stage: str) -> tuple[str, str, int]:
    field, separator, raw_value = text.partition("=")
    if not separator:
        raise Refusal("--force-port-field must be [STAGE:]NAME=VALUE")
    if ":" in field:
        stage, name = field.split(":", 1)
    else:
        stage, name = default_stage, field
    if (
        stage not in {"first", "modeled", "post", "resident"}
        or name not in REGISTER_NAMES
    ):
        raise Refusal(
            "--force-port-field must name first, modeled, post, or resident and one boundary register"
        )
    try:
        return stage, name, int(raw_value, 0)
    except ValueError as error:
        raise Refusal(f"invalid forced value {raw_value!r}") from error


def force_field(
    evidence: PostInitHeapEvidence, stage: str, name: str, value: int
) -> PostInitHeapEvidence:
    if stage == "first":
        return dataclasses.replace(
            evidence,
            first_call=dataclasses.replace(
                evidence.first_call,
                registers={**evidence.first_call.registers, name: value},
            ),
        )
    if stage == "modeled":
        model = evidence.modeled_return
        return dataclasses.replace(
            evidence,
            modeled_return=dataclasses.replace(
                model,
                boundary=dataclasses.replace(
                    model.boundary, registers={**model.boundary.registers, name: value}
                ),
            ),
        )
    return dataclasses.replace(
        evidence,
        post_return_call=dataclasses.replace(
            evidence.post_return_call,
            registers={**evidence.post_return_call.registers, name: value},
        ),
    )


def capture_deterministic_post_init(
    base_command: list[str], output: pathlib.Path, repeat_output: pathlib.Path
) -> PostInitHeapEvidence:
    command = [*base_command, "--model-bios-return", "A:0x39:0"]
    run([*command, "--out", str(output)], "oracle post-InitHeap trace A")
    run([*command, "--out", str(repeat_output)], "oracle post-InitHeap trace B")
    first = parse_post_init_heap(output.read_text(encoding="utf-8"), port=False)
    repeat = parse_post_init_heap(repeat_output.read_text(encoding="utf-8"), port=False)
    if first != repeat:
        raise Refusal(
            "two independent oracle runs produced different boundary state or step counts"
        )
    return first


def resident_state_arguments(boundary: Boundary) -> str:
    values = [boundary.registers[name] for name in REGISTER_NAMES]
    return ",".join(f"0x{value:08X}" for value in values)


def replay_stack_writes(stack_pointer: int, runtime_init: bool) -> tuple[range, ...]:
    required = 76 if runtime_init else 32
    if stack_pointer < required:
        raise Refusal("resident stack-store range wraps below address zero")
    ranges = (range(stack_pointer - 32, stack_pointer - 4),)
    if runtime_init:
        ranges += (range(stack_pointer - 76, stack_pointer - 64),)
    return ranges


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", nargs="?")
    parser.add_argument("--oracle-trace")
    parser.add_argument("--port-trace")
    parser.add_argument("--steps", type=int, default=400000)
    parser.add_argument("--post-init-heap", action="store_true")
    parser.add_argument("--resident-next-call", action="store_true")
    parser.add_argument("--runtime-init-next-call", action="store_true")
    parser.add_argument("--startup-service-next-call", action="store_true")
    parser.add_argument("--startup-memset-thunk", action="store_true")
    parser.add_argument("--startup-post-memset-next-call", action="store_true")
    parser.add_argument("--startup-init-swap-next-call", action="store_true")
    parser.add_argument("--startup-init-dispatch-next-call", action="store_true")
    parser.add_argument("--startup-init-device-next-call", action="store_true")
    parser.add_argument("--startup-zero-fill-next-call", action="store_true")
    parser.add_argument(
        "--force-port-field", help="test-only post-capture mutation, [STAGE:]NAME=VALUE"
    )
    parser.add_argument("--expect-difference", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()

    if arguments.selftest:
        if arguments.exe or arguments.oracle_trace or arguments.port_trace:
            raise Refusal("--selftest takes no executable or trace tools")
        return selftest()
    if not arguments.exe or not arguments.oracle_trace or not arguments.port_trace:
        raise Refusal(
            "an executable, --oracle-trace, and --port-trace are all required"
        )
    if arguments.steps <= 0:
        raise Refusal("--steps must be positive; an empty run is not agreement")
    selected_windows = sum(
        (
            arguments.post_init_heap,
            arguments.resident_next_call,
            arguments.runtime_init_next_call,
            arguments.startup_service_next_call,
            arguments.startup_memset_thunk,
            arguments.startup_post_memset_next_call,
            arguments.startup_init_swap_next_call,
            arguments.startup_init_dispatch_next_call,
            arguments.startup_init_device_next_call,
            arguments.startup_zero_fill_next_call,
        )
    )
    if selected_windows > 1:
        raise Refusal(
            "post-InitHeap and each resident next-call option are distinct evidence windows"
        )

    exe = pathlib.Path(arguments.exe)
    oracle_tool = pathlib.Path(arguments.oracle_trace)
    port_tool = pathlib.Path(arguments.port_trace)
    for path, label in (
        (exe, "executable"),
        (oracle_tool, "oracle tracer"),
        (port_tool, "port tracer"),
    ):
        if not path.is_file():
            raise Refusal(f"{label} does not exist: {path}")

    scratch = pathlib.Path(__file__).resolve().parents[1] / "scratch" / "logs"
    scratch.mkdir(parents=True, exist_ok=True)
    oracle_output = scratch / "ctr04-oracle-boundary.trace"
    base_oracle_command = [
        str(oracle_tool),
        str(exe),
        "--steps",
        str(arguments.steps),
        "--capture-call",
        "1",
        "--summary-only",
    ]

    if (
        arguments.resident_next_call
        or arguments.runtime_init_next_call
        or arguments.startup_service_next_call
        or arguments.startup_memset_thunk
        or arguments.startup_post_memset_next_call
        or arguments.startup_init_swap_next_call
        or arguments.startup_init_dispatch_next_call
        or arguments.startup_init_device_next_call
        or arguments.startup_zero_fill_next_call
    ):
        include_zero_fill = arguments.startup_zero_fill_next_call
        include_init_device = (
            arguments.startup_init_device_next_call or include_zero_fill
        )
        include_init_dispatch = (
            arguments.startup_init_dispatch_next_call or include_init_device
        )
        include_init_swap = (
            arguments.startup_init_swap_next_call or include_init_dispatch
        )
        include_post_memset = (
            arguments.startup_post_memset_next_call or include_init_swap
        )
        include_memset_thunk = arguments.startup_memset_thunk or include_post_memset
        include_startup_service = (
            arguments.startup_service_next_call or include_memset_thunk
        )
        include_runtime_init = (
            arguments.runtime_init_next_call or include_startup_service
        )
        if include_zero_fill:
            label = "ctr04 startup-zero-fill-next-call compare"
        elif include_init_device:
            label = "ctr04 startup-init-device-next-call compare"
        elif include_init_dispatch:
            label = "ctr04 startup-init-dispatch-next-call compare"
        elif include_init_swap:
            label = "ctr04 startup-init-swap-next-call compare"
        elif include_post_memset:
            label = "ctr04 startup-post-memset-next-call compare"
        elif include_memset_thunk:
            label = "ctr04 startup-memset-thunk compare"
        elif include_startup_service:
            label = "ctr04 startup-service-next-call compare"
        elif include_runtime_init:
            label = "ctr04 runtime-init-next-call compare"
        else:
            label = "ctr04 resident-next-call compare"
        oracle_repeat_output = scratch / "ctr04-oracle-boundary-repeat.trace"
        post = capture_deterministic_post_init(
            base_oracle_command, oracle_output, oracle_repeat_output
        )
        if post.post_return_call.target != RESIDENT_RESUME_TARGET:
            raise Refusal(
                f"post-InitHeap oracle reached 0x{post.post_return_call.target:08X}, not the "
                f"proven resident resume 0x{RESIDENT_RESUME_TARGET:08X}"
            )
        print(
            f"{label}: determinism PASS — two original oracle runs "
            "produced identical post-InitHeap state"
        )

        gprs = (
            0,
            *(post.post_return_call.registers[name] for name in REGISTER_NAMES[:-2]),
        )
        stack_pointer = post.post_return_call.registers["sp"]
        replay_path = scratch.parent / "raw" / "ctr" / "ctr04-resident-replay.exe"
        expected_ranges = ()
        expected_words = ()
        forbidden_ranges = replay_stack_writes(stack_pointer, include_runtime_init)
        if include_runtime_init:
            expected_ranges = (
                (RUNTIME_INIT_TARGET, RUNTIME_INIT_CODE),
                (POST_RUNTIME_INIT_TARGET, POST_RUNTIME_INIT_CODE),
            )
            expected_words = ((RUNTIME_INIT_FLAG, 0), (INITIAL_MODE_WORD, 0))
        if include_startup_service:
            expected_ranges += (
                (STARTUP_SERVICE_ENTRY, STARTUP_SERVICE_ENTRY_CODE),
                (STARTUP_SERVICE_IDLE, STARTUP_SERVICE_IDLE_CODE),
                (STARTUP_SERVICE_RETURN, STARTUP_SERVICE_RETURN_CODE),
                (POST_STARTUP_SERVICE, POST_STARTUP_SERVICE_CODE),
            )
            expected_words += (
                (STARTUP_SERVICE_REQUEST_WORD, 1),
                (STARTUP_SERVICE_LOADING_FLAG, 0),
                (STARTUP_SERVICE_TIMESTAMP, 0),
            )
        if include_memset_thunk:
            expected_ranges += (
                (BACKGROUND_SERVICE, BACKGROUND_SERVICE_ENTRY_CODE),
                (BACKGROUND_SERVICE_RETURN, BACKGROUND_SERVICE_RETURN_CODE),
                (POST_BACKGROUND_SERVICE, STATE_DISPATCH_CODE),
                (STATE_ZERO_CASE, STATE_ZERO_CASE_CODE),
            )
            expected_words += (
                (BACKGROUND_SERVICE_PENDING, 0),
                (STATE_JUMP_TABLE_ZERO, STATE_ZERO_CASE),
                (GLOBAL_STATE_POINTER, GLOBAL_STATE_BASE),
            )
        if include_post_memset:
            expected_ranges += (
                (STARTUP_MEMSET_THUNK, STARTUP_MEMSET_THUNK_CODE),
                (POST_STARTUP_MEMSET, POST_STARTUP_MEMSET_CODE),
            )
        if include_init_swap:
            expected_ranges += (
                (POST_INIT_SWAP, POST_INIT_SWAP_CODE),
                (STARTUP_INIT_SWAP, STARTUP_INIT_SWAP_CODE),
            )
            expected_words += ((INIT_SWAP_DATA_WORD, 0),)
        if include_init_dispatch:
            expected_ranges += ((INIT_DISPATCHER, INIT_DISPATCHER_CODE),)
            expected_words += (
                (INIT_DISPATCH_TABLE, INIT_DISPATCH_TABLE_BASE),
                (INIT_DISPATCH_SLOT, INIT_NEXT_CALL),
            )
        if include_init_device:
            expected_ranges += ((INIT_NEXT_CALL, INITIALIZER_DEVICE_PREFIX),)
        try:
            replay = write_replay(
                exe,
                replay_path,
                resume_target=RESIDENT_RESUME_TARGET,
                registers=gprs,
                lo=post.post_return_call.registers["lo"],
                hi=post.post_return_call.registers["hi"],
                expected_prefix=RESIDENT_PREFIX,
                expected_ranges=expected_ranges,
                expected_words=expected_words,
                forbidden_ranges=forbidden_ranges,
                modeled_memset=(
                    ModeledMemset(
                        thunk=STARTUP_MEMSET_THUNK,
                        expected_thunk=STARTUP_MEMSET_THUNK_CODE,
                        destination=GLOBAL_STATE_BASE,
                        value=0,
                        size=0x2584,
                    )
                    if include_post_memset
                    else None
                ),
                observed_zero_fill=(
                    ObservedZeroFill(
                        callsite=ZERO_FILL_CALLSITE,
                        expected_callsite=ZERO_FILL_CALLSITE_CODE,
                        target=ZERO_FILL_TARGET,
                        expected_body=ZERO_FILL_BODY,
                        return_pc=ZERO_FILL_RETURN,
                        expected_return=ZERO_FILL_RETURN_CODE,
                        destination=ZERO_FILL_DESTINATION,
                        words=ZERO_FILL_WORDS,
                        next_target=ZERO_FILL_NEXT_CALL,
                        next_ra=ZERO_FILL_RETURN + 8,
                        next_a0=0x38,
                        poison=ZERO_FILL_POISON,
                    )
                    if include_zero_fill
                    else None
                ),
            )
        except ReplayRefusal as error:
            raise Refusal(f"resident replay construction refused: {error}") from error
        print(
            f"{label}: bounded replay trampoline "
            f"0x{replay.trampoline:08X} ({replay.size} bytes)"
        )

        resident_output = scratch / "ctr04-resident-oracle.trace"
        resident_repeat_output = scratch / "ctr04-resident-oracle-repeat.trace"
        call_ordinal = (
            6
            if include_init_swap
            else 5
            if include_post_memset
            else 4
            if include_memset_thunk
            else (3 if include_startup_service else (2 if include_runtime_init else 1))
        )
        resident_command = [
            str(oracle_tool),
            str(replay_path),
            "--steps",
            "80000" if include_zero_fill else "60000" if include_post_memset else "256",
            *(
                [
                    "--capture-at",
                    f"0x{ZERO_FILL_NEXT_CALL if include_zero_fill else INITIALIZER_DEVICE_NEXT_CALL if include_init_device else INIT_NEXT_CALL:08X}",
                    *(["--capture-devices"] if include_init_device else []),
                ]
                if include_init_dispatch
                else ["--capture-call", str(call_ordinal)]
            ),
            "--summary-only",
        ]
        run(
            [*resident_command, "--out", str(resident_output)],
            "resident replay oracle trace A",
        )
        run(
            [*resident_command, "--out", str(resident_repeat_output)],
            "resident replay oracle trace B",
        )
        boundary_parser = (
            (lambda text: parse_pc_boundary(text, "oracle"))
            if include_init_dispatch
            else (lambda text: parse_boundary(text, port=False))
        )
        resident_text = resident_output.read_text(encoding="utf-8")
        repeat_text = resident_repeat_output.read_text(encoding="utf-8")
        oracle_boundary = boundary_parser(resident_text)
        oracle_repeat = boundary_parser(repeat_text)
        oracle_devices = (
            parse_device_boundary(
                resident_text, port=False, expected_pc=oracle_boundary.pc
            )
            if include_init_device
            else None
        )
        oracle_repeat_devices = (
            parse_device_boundary(repeat_text, port=False, expected_pc=oracle_repeat.pc)
            if include_init_device
            else None
        )
        oracle_memory = (
            MemoryBoundary(
                oracle_boundary.pc,
                ZERO_FILL_DESTINATION,
                ZERO_FILL_WORDS * 4,
                ZERO_FILL_POISON,
                oracle_boundary.registers["k0"],
            )
            if include_zero_fill
            else None
        )
        oracle_repeat_memory = (
            MemoryBoundary(
                oracle_repeat.pc,
                ZERO_FILL_DESTINATION,
                ZERO_FILL_WORDS * 4,
                ZERO_FILL_POISON,
                oracle_repeat.registers["k0"],
            )
            if include_zero_fill
            else None
        )
        if (
            oracle_boundary != oracle_repeat
            or oracle_devices != oracle_repeat_devices
            or oracle_memory != oracle_repeat_memory
        ):
            raise Refusal(
                "two resident replay oracle runs produced different boundary state or steps"
            )
        expected_target = (
            ZERO_FILL_NEXT_CALL
            if include_zero_fill
            else INITIALIZER_DEVICE_NEXT_CALL
            if include_init_device
            else INIT_NEXT_CALL
            if include_init_dispatch
            else INIT_DISPATCHER
            if include_init_swap
            else STARTUP_MEMSET_NEXT_CALL
            if include_post_memset
            else STARTUP_MEMSET_THUNK
            if include_memset_thunk
            else (
                STARTUP_SERVICE_NEXT_CALL
                if include_startup_service
                else RUNTIME_INIT_NEXT_CALL
            )
        )
        if include_runtime_init and oracle_boundary.target != expected_target:
            raise Refusal(
                f"resident replay reached 0x{oracle_boundary.target:08X}, not the "
                f"Ghidra-proven next call 0x{expected_target:08X}"
            )
        print(
            f"{label}: determinism PASS — two replay oracle runs "
            "produced identical call-boundary evidence"
        )

        port_result = run(
            [
                str(port_tool),
                str(replay_path if include_post_memset else exe),
                "--resume-target",
                f"0x{RESIDENT_RESUME_TARGET:08X}",
                "--capture-target",
                f"0x{oracle_boundary.target:08X}",
                "--state",
                resident_state_arguments(post.post_return_call),
                *(
                    [
                        "--model-memset",
                        f"0x{STARTUP_MEMSET_THUNK:08X},0x{GLOBAL_STATE_BASE:08X},0,0x2584,0xA5",
                    ]
                    if include_post_memset
                    else []
                ),
                *(
                    [
                        "--probe-zero-fill",
                        (
                            f"0x{ZERO_FILL_TARGET:08X},0x{ZERO_FILL_DESTINATION:08X},"
                            f"0x{ZERO_FILL_WORDS:X},0x{ZERO_FILL_POISON:02X}"
                        ),
                    ]
                    if include_zero_fill
                    else []
                ),
                *(["--capture-devices"] if include_init_device else []),
            ],
            "generated resident replay trace",
        )
        port_boundary = parse_boundary(port_result.stdout, port=True)
        port_devices = (
            parse_device_boundary(
                port_result.stdout, port=True, expected_pc=port_boundary.pc
            )
            if include_init_device
            else None
        )
        port_memory = (
            parse_memory_boundary(port_result.stdout, expected_pc=port_boundary.pc)
            if include_zero_fill
            else None
        )
        if arguments.force_port_field:
            field, separator, raw_value = arguments.force_port_field.partition("=")
            if include_zero_fill and separator and field == "memory:nonzero_words":
                assert port_memory is not None
                try:
                    value = int(raw_value, 0)
                except ValueError as error:
                    raise Refusal(f"invalid forced value {raw_value!r}") from error
                if value < 0 or value > ZERO_FILL_WORDS:
                    raise Refusal(
                        "forced memory nonzero_words exceeds the observed range"
                    )
                port_memory = dataclasses.replace(port_memory, nonzero_words=value)
                stage = "memory"
                name = "nonzero_words"
            elif include_init_device and separator and field.startswith("device:"):
                assert port_devices is not None
                name = field.removeprefix("device:")
                if name not in port_devices.values:
                    raise Refusal(
                        "device forced field must name I_STAT, I_MASK, or DPCR"
                    )
                try:
                    value = int(raw_value, 0)
                except ValueError as error:
                    raise Refusal(f"invalid forced value {raw_value!r}") from error
                port_devices = dataclasses.replace(
                    port_devices, values={**port_devices.values, name: value}
                )
                stage = "device"
            else:
                stage, name, value = parse_forced_field(
                    arguments.force_port_field, "resident"
                )
                if stage != "resident":
                    raise Refusal(
                        "first/modeled/post forced fields do not belong to resident replay"
                    )
                port_boundary = dataclasses.replace(
                    port_boundary, registers={**port_boundary.registers, name: value}
                )
            print(f"{label}: TEST-ONLY forced generated {stage}.{name}=0x{value:08X}")
        rows = compare_boundary(oracle_boundary, port_boundary, "resident")
        if include_init_device:
            assert oracle_devices is not None and port_devices is not None
            rows.extend(compare_devices(oracle_devices, port_devices))
        if include_zero_fill:
            assert oracle_memory is not None and port_memory is not None
            rows.extend(compare_memory(oracle_memory, port_memory))
        comparison_label = label
    elif arguments.post_init_heap:
        oracle_repeat_output = scratch / "ctr04-oracle-boundary-repeat.trace"
        oracle = capture_deterministic_post_init(
            base_oracle_command, oracle_output, oracle_repeat_output
        )
        print(
            "ctr04 post-InitHeap compare: determinism PASS — two oracle runs produced identical three-boundary evidence"
        )

        port_result = run(
            [
                str(port_tool),
                str(exe),
                "--target",
                f"0x{oracle.first_call.target:08X}",
                "--model-init-heap-return",
                "--post-target",
                f"0x{oracle.post_return_call.target:08X}",
            ],
            "generated post-InitHeap port trace",
        )
        port = parse_post_init_heap(port_result.stdout, port=True)
        if arguments.force_port_field:
            stage, name, value = parse_forced_field(arguments.force_port_field, "post")
            port = force_field(port, stage, name, value)
            print(
                f"ctr04 post-InitHeap compare: TEST-ONLY forced generated {stage}.{name}=0x{value:08X}"
            )
        rows = compare_post_init_heap(oracle, port)
        comparison_label = "ctr04 post-InitHeap compare"
    else:
        run([*base_oracle_command, "--out", str(oracle_output)], "oracle trace")
        oracle_boundary = parse_boundary(
            oracle_output.read_text(encoding="utf-8"), port=False
        )
        port_result = run(
            [str(port_tool), str(exe), "--target", f"0x{oracle_boundary.target:08X}"],
            "generated port trace",
        )
        port_boundary = parse_boundary(port_result.stdout, port=True)
        if arguments.force_port_field:
            stage, name, value = parse_forced_field(arguments.force_port_field, "first")
            if stage != "first":
                raise Refusal("modeled/post forced fields require --post-init-heap")
            port_boundary = dataclasses.replace(
                port_boundary, registers={**port_boundary.registers, name: value}
            )
            print(f"ctr04 compare: TEST-ONLY forced generated {name}=0x{value:08X}")
        rows = compare(oracle_boundary, port_boundary)
        comparison_label = "ctr04 compare"

    differences = print_rows(rows, comparison_label)
    if arguments.expect_difference:
        if differences == 0:
            raise Refusal(
                "--expect-difference was requested but the comparator reported agreement"
            )
        print(
            f"{comparison_label}: PASS — the forced opposite produced a named disagreement"
        )
        return 0
    return 1 if differences else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Refusal as error:
        print(f"compare_crt0_trace: REFUSING — {error}", file=sys.stderr)
        print("Nothing was compared. This is not agreement.", file=sys.stderr)
        sys.exit(2)
