"""Both-answer fixtures for the shipping CTR trace comparator."""

from __future__ import annotations

import dataclasses
import struct

from compare_crt0_trace import (
    BACKGROUND_SERVICE_PENDING,
    BSS_START,
    GLOBAL_STATE_POINTER,
    INIT_DISPATCH_SLOT,
    INIT_DISPATCH_TABLE,
    INIT_DISPATCH_TABLE_BASE,
    INIT_SWAP_DATA_WORD,
    INITIAL_MODE_WORD,
    REGISTER_NAMES,
    RESIDENT_PREFIX,
    RUNTIME_INIT_FLAG,
    STARTUP_SERVICE_LOADING_FLAG,
    STARTUP_SERVICE_REQUEST_WORD,
    STARTUP_SERVICE_TIMESTAMP,
    STATE_JUMP_TABLE_ZERO,
    ZERO_FILL_BODY,
    ZERO_FILL_DESTINATION,
    ZERO_FILL_POISON,
    ZERO_FILL_WORDS,
    Boundary,
    DeviceBoundary,
    MemoryBoundary,
    ModeledMemset,
    ModeledReturn,
    ObservedZeroFill,
    PostInitHeapEvidence,
    Refusal,
    ReplayRefusal,
    compare_boundary,
    compare_devices,
    compare_memory,
    compare_post_init_heap,
    pack_words,
    parse_device_boundary,
    parse_memory_boundary,
    parse_pc_boundary,
    parse_post_init_heap,
    replay_stack_writes,
)
from resident_replay import build_replay


def format_boundary(boundary: Boundary, capture_tag: str, register_tag: str) -> str:
    step = f" step={boundary.step}" if boundary.step is not None else ""
    lines = [
        f"# {capture_tag} target=0x{boundary.target:08X} ra=0x{boundary.registers['ra']:08X}{step}",
        f"# {register_tag}-REGS{step} pc=0x{boundary.pc:08X}",
    ]
    lines.extend(
        f"# {register_tag}-REG {name}=0x{boundary.registers[name]:08X}"
        for name in REGISTER_NAMES
    )
    return "\n".join(lines)


def format_pc_boundary(boundary: Boundary) -> str:
    if boundary.step is None:
        raise ValueError(
            "PC-boundary fixtures require an executed-instruction denominator"
        )
    lines = [
        f"# CAPTURED-PC target=0x{boundary.target:08X} executed={boundary.step}",
        f"# PC-BOUNDARY-REGS executed={boundary.step} pc=0x{boundary.pc:08X}",
    ]
    lines.extend(
        f"# PC-BOUNDARY-REG {name}=0x{boundary.registers[name]:08X}"
        for name in REGISTER_NAMES
    )
    return "\n".join(lines)


def format_device_boundary(boundary: DeviceBoundary, port: bool) -> str:
    prefix = "PORT-" if port else ""
    lines = [f"# {prefix}DEVICE-BOUNDARY schema=1 pc=0x{boundary.pc:08X}"]
    for name, mask in (("I_STAT", 0x7FF), ("I_MASK", 0x7FF), ("DPCR", 0xFFFFFFFF)):
        lines.append(
            f"# {prefix}DEVICE-REG {name} value=0x{boundary.values[name]:08X} mask=0x{mask:08X}"
        )
    return "\n".join(lines)


def format_evidence(evidence: PostInitHeapEvidence, port: bool) -> str:
    prefix = "PORT-" if port else ""
    model = evidence.modeled_return
    step = f" step={model.step}" if model.step is not None else ""
    model_lines = [
        (
            f"# {prefix}MODELED-BIOS-RETURN table={model.table} "
            f"function=0x{model.function:02X} target=0x{model.target:08X} "
            f"ra=0x{model.return_pc:08X} v0=0x{model.v0:08X} "
            f"v1=0x{model.v1:08X}{step}"
        ),
        f"# {prefix}MODELED-RETURN-REGS{step} pc=0x{model.boundary.pc:08X}",
    ]
    model_lines.extend(
        f"# {prefix}MODELED-RETURN-REG {name}=0x{model.boundary.registers[name]:08X}"
        for name in REGISTER_NAMES
    )
    return "\n".join(
        (
            format_boundary(
                evidence.first_call, f"{prefix}CAPTURED-CALL", f"{prefix}CALL-BOUNDARY"
            ),
            *model_lines,
            format_boundary(
                evidence.post_return_call,
                f"{prefix}POST-RETURN-CAPTURED-CALL",
                f"{prefix}POST-RETURN-CALL-BOUNDARY",
            ),
        )
    )


def selftest() -> int:
    registers = {name: index + 1 for index, name in enumerate(REGISTER_NAMES)}
    first = Boundary(0x80001000, 0x80001000, registers, 100)
    modeled_boundary = Boundary(
        0x80001008, 0x80001008, {**registers, "v0": 0, "t1": 0x39, "t2": 0xA0}, 103
    )
    model = ModeledReturn(
        "A", 0x39, 0xA0, 0x80001008, 0, registers["v1"], modeled_boundary, 103
    )
    post = Boundary(
        0x80002000, 0x80002000, {**modeled_boundary.registers, "ra": 0x80001020}, 108
    )
    expected = PostInitHeapEvidence(first, model, post)
    oracle = parse_post_init_heap(format_evidence(expected, port=False), port=False)
    port = parse_post_init_heap(
        format_evidence(
            dataclasses.replace(
                expected,
                first_call=dataclasses.replace(first, step=None),
                modeled_return=dataclasses.replace(
                    model,
                    boundary=dataclasses.replace(modeled_boundary, step=None),
                    step=None,
                ),
                post_return_call=dataclasses.replace(post, step=None),
            ),
            port=True,
        ),
        port=True,
    )
    rows = compare_post_init_heap(oracle, port)
    expected_devices = DeviceBoundary(
        post.pc, {"I_STAT": 0, "I_MASK": 0, "DPCR": 0x33333333}
    )
    oracle_devices = parse_device_boundary(
        format_device_boundary(expected_devices, port=False),
        port=False,
        expected_pc=post.pc,
    )
    port_devices = parse_device_boundary(
        format_device_boundary(expected_devices, port=True),
        port=True,
        expected_pc=post.pc,
    )
    forced = dataclasses.replace(
        port,
        post_return_call=dataclasses.replace(
            port.post_return_call,
            registers={**port.post_return_call.registers, "gp": 0},
        ),
    )
    checks = [
        ("complete three-boundary evidence parses", oracle == expected),
        (
            "pre-instruction PC boundary parses with its executed denominator",
            parse_pc_boundary(format_pc_boundary(post), "fixture") == post,
        ),
        (
            "equal oracle and generated evidence",
            not any(left != right for _, left, right in rows),
        ),
        (
            "forced opposite is visible",
            sum(
                left != right
                for _, left, right in compare_post_init_heap(oracle, forced)
            )
            == 1,
        ),
        (
            "repeat-run nondeterminism is visible",
            oracle
            != dataclasses.replace(
                oracle, post_return_call=dataclasses.replace(post, step=109)
            ),
        ),
        (
            "complete fixed-schema device evidence parses",
            oracle_devices == expected_devices and port_devices == expected_devices,
        ),
        (
            "forced DPCR opposite is visible without changing CPU evidence",
            sum(
                left != right
                for _, left, right in compare_devices(
                    oracle_devices,
                    dataclasses.replace(
                        port_devices,
                        values={**port_devices.values, "DPCR": 0x33333332},
                    ),
                )
            )
            == 1
            and not any(
                left != right
                for _, left, right in compare_boundary(post, post, "resident")
            ),
        ),
    ]
    expected_memory = MemoryBoundary(
        post.pc,
        ZERO_FILL_DESTINATION,
        ZERO_FILL_WORDS * 4,
        ZERO_FILL_POISON,
        0,
    )
    memory_fixture = (
        f"# PORT-MEMORY-BOUNDARY schema=1 pc=0x{post.pc:08X} "
        f"destination=0x{ZERO_FILL_DESTINATION:08X} size=0x{ZERO_FILL_WORDS * 4:08X} "
        f"poison=0x{ZERO_FILL_POISON:02X}\n"
        "# PORT-MEMORY-RESULT nonzero_words=0x00000000\n"
    )
    parsed_memory = parse_memory_boundary(memory_fixture, post.pc)
    checks.extend(
        (
            (
                "complete zero-fill memory evidence parses",
                parsed_memory == expected_memory,
            ),
            (
                "forced zero-fill residue is visible",
                sum(
                    left != right
                    for _, left, right in compare_memory(
                        expected_memory,
                        dataclasses.replace(parsed_memory, nonzero_words=1),
                    )
                )
                == 1,
            ),
        )
    )
    try:
        parse_post_init_heap(
            format_boundary(first, "CAPTURED-CALL", "CALL-BOUNDARY"), port=False
        )
    except Refusal:
        checks.append(("missing modeled/post boundary refuses", True))
    else:
        checks.append(("missing modeled/post boundary refuses", False))
    mismatched_ra = format_evidence(expected, port=False).replace(
        "ra=0x0000001F", "ra=0xDEADBEEF", 1
    )
    try:
        parse_post_init_heap(mismatched_ra, port=False)
    except Refusal:
        checks.append(("capture metadata/register disagreement refuses", True))
    else:
        checks.append(("capture metadata/register disagreement refuses", False))
    malformed_devices = format_device_boundary(expected_devices, port=False).replace(
        "mask=0x000007FF", "mask=0xFFFFFFFF", 1
    )
    try:
        parse_device_boundary(malformed_devices, port=False, expected_pc=post.pc)
    except Refusal:
        checks.append(("malformed device mask refuses", True))
    else:
        checks.append(("malformed device mask refuses", False))
    missing_device = "\n".join(
        line
        for line in format_device_boundary(expected_devices, port=False).splitlines()
        if " I_MASK " not in line
    )
    try:
        parse_device_boundary(missing_device, port=False, expected_pc=post.pc)
    except Refusal:
        checks.append(("missing device register refuses", True))
    else:
        checks.append(("missing device register refuses", False))

    synthetic = bytearray(0x800 + 0x800)
    synthetic[:8] = b"PS-X EXE"
    struct.pack_into("<I", synthetic, 0x10, 0x80010000)
    struct.pack_into("<I", synthetic, 0x18, 0x80010000)
    struct.pack_into("<I", synthetic, 0x1C, 0x800)
    prefix_offset = 0x300
    synthetic[0x800 + prefix_offset : 0x800 + prefix_offset + len(RESIDENT_PREFIX)] = (
        RESIDENT_PREFIX
    )
    continuation_address = 0x80010500
    continuation = bytes.fromhex("11223344")
    synthetic[0x800 + 0x500 : 0x800 + 0x500 + len(continuation)] = continuation
    initialized_word_address = 0x80010600
    initialized_word = 0x78563412
    struct.pack_into("<I", synthetic, 0x800 + 0x600, initialized_word)
    replay_registers = (0, *range(1, 26), 0, 0, 28, 29, 30, 31)
    replay_a = build_replay(
        bytes(synthetic),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0x12345678,
        hi=0x9ABCDEF0,
        expected_prefix=RESIDENT_PREFIX,
        expected_ranges=((continuation_address, continuation),),
        expected_words=((initialized_word_address, initialized_word),),
    )
    replay_b = build_replay(
        bytes(synthetic),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0x12345678,
        hi=0x9ABCDEF0,
        expected_prefix=RESIDENT_PREFIX,
        expected_ranges=((continuation_address, continuation),),
        expected_words=((initialized_word_address, initialized_word),),
    )
    checks.append(
        (
            "resident replay construction is deterministic",
            replay_a == replay_b
            and struct.unpack_from("<I", replay_a.data, 0x10)[0] == replay_a.trampoline,
        )
    )
    aliased = build_replay(
        bytes(synthetic),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0x12345678,
        hi=0x9ABCDEF0,
        expected_prefix=RESIDENT_PREFIX,
        forbidden_ranges=(
            range(
                replay_a.trampoline + 0x200000,
                replay_a.trampoline + 0x200000 + replay_a.size,
            ),
        ),
    )
    checks.append(
        (
            "main-RAM alias exclusion moves the trampoline",
            aliased.trampoline != replay_a.trampoline,
        )
    )
    evidence_protected = build_replay(
        bytes(synthetic),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0,
        hi=0,
        expected_prefix=RESIDENT_PREFIX,
        expected_words=((replay_a.trampoline, 0),),
    )
    checks.append(
        (
            "checked memory inputs cannot be occupied by the replay trampoline",
            evidence_protected.trampoline != replay_a.trampoline,
        )
    )
    changed = bytearray(synthetic)
    changed[0x800 + prefix_offset] ^= 1
    try:
        build_replay(
            bytes(changed),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
        )
    except ReplayRefusal:
        checks.append(("changed resident prefix refuses", True))
    else:
        checks.append(("changed resident prefix refuses", False))
    changed_continuation = bytearray(synthetic)
    changed_continuation[0x800 + 0x500] ^= 1
    try:
        build_replay(
            bytes(changed_continuation),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
            expected_ranges=((continuation_address, continuation),),
        )
    except ReplayRefusal:
        checks.append(("changed non-contiguous continuation refuses", True))
    else:
        checks.append(("changed non-contiguous continuation refuses", False))
    changed_data = bytearray(synthetic)
    changed_data[0x800 + 0x600] ^= 1
    try:
        build_replay(
            bytes(changed_data),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
            expected_words=((initialized_word_address, initialized_word),),
        )
    except ReplayRefusal:
        checks.append(("changed initialized-data input refuses", True))
    else:
        checks.append(("changed initialized-data input refuses", False))
    no_zero_run = bytearray(synthetic)
    no_zero_run[0x800:] = bytes([0xA5]) * 0x800
    no_zero_run[
        0x800 + prefix_offset : 0x800 + prefix_offset + len(RESIDENT_PREFIX)
    ] = RESIDENT_PREFIX
    try:
        build_replay(
            bytes(no_zero_run),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
        )
    except ReplayRefusal:
        checks.append(("missing aligned zero run refuses", True))
    else:
        checks.append(("missing aligned zero run refuses", False))
    no_scratch = (0, *range(1, 32))
    try:
        build_replay(
            bytes(synthetic),
            resume_target=0x80010000 + prefix_offset,
            registers=no_scratch,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
        )
    except ReplayRefusal:
        checks.append(("missing exact scratch register refuses", True))
    else:
        checks.append(("missing exact scratch register refuses", False))
    stack_ranges = replay_stack_writes(0x1000, runtime_init=True)
    checks.append(
        (
            "all outer and runtime-init stack-store bytes are excluded",
            stack_ranges == (range(0xFE0, 0xFFC), range(0xFB4, 0xFC0)),
        )
    )
    checks.append(
        (
            "runtime-init inputs remain initialized data before BSS",
            RUNTIME_INIT_FLAG < BSS_START and INITIAL_MODE_WORD < BSS_START,
        )
    )
    checks.append(
        (
            "startup-service replay distinguishes initialized state from BSS zero",
            STARTUP_SERVICE_REQUEST_WORD < BSS_START
            and STARTUP_SERVICE_TIMESTAMP < BSS_START
            and STARTUP_SERVICE_LOADING_FLAG >= BSS_START,
        )
    )
    checks.append(
        (
            "state-zero service inputs retain their executable/BSS classification",
            STATE_JUMP_TABLE_ZERO < BSS_START
            and GLOBAL_STATE_POINTER < BSS_START
            and BACKGROUND_SERVICE_PENDING >= BSS_START,
        )
    )
    checks.append(
        (
            "init-swap dispatch inputs remain initialized data before BSS",
            INIT_SWAP_DATA_WORD < BSS_START
            and INIT_DISPATCH_TABLE < BSS_START
            and INIT_DISPATCH_TABLE_BASE < BSS_START
            and INIT_DISPATCH_SLOT < BSS_START,
        )
    )
    modeled_memset = ModeledMemset(
        thunk=0x80010100,
        expected_thunk=bytes(synthetic[0x900:0x90C]),
        destination=0x80010800,
        value=0,
        size=0x40,
    )
    modeled = build_replay(
        bytes(synthetic),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0,
        hi=0,
        expected_prefix=RESIDENT_PREFIX,
        modeled_memset=modeled_memset,
    )
    modeled_payload = modeled.data[0x800:]
    modeled_destination = modeled_memset.destination - 0x80010000
    assert modeled.leaf_model is not None
    modeled_code_offset = modeled.leaf_model - 0x80010000
    modeled_code = modeled_payload[
        modeled_code_offset : modeled_code_offset + modeled.leaf_model_size
    ]
    checks.extend(
        (
            (
                "modeled memset poisons a BSS destination to make a missing write observable",
                modeled_payload[
                    modeled_destination : modeled_destination + modeled_memset.size
                ]
                == bytes([modeled_memset.poison]) * modeled_memset.size,
            ),
            (
                "modeled memset redirects the checked thunk to its executable model",
                modeled_payload[0x100:0x10C]
                == pack_words(
                    (0x08000000 | ((modeled.leaf_model >> 2) & 0x03FFFFFF), 0, 0)
                ),
            ),
            (
                "modeled memset preserves temporaries in its destination, never guest stack",
                modeled_code.startswith(
                    pack_words((0xAC830000, 0xAC880004, 0xAC890008))
                )
                and pack_words((0x27BDFFF0,)) not in modeled_code
                and pack_words((0xAFA30000,)) not in modeled_code,
            ),
        )
    )
    changed_thunk = bytearray(synthetic)
    changed_thunk[0x900] ^= 1
    try:
        build_replay(
            bytes(changed_thunk),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
            modeled_memset=modeled_memset,
        )
    except ReplayRefusal:
        checks.append(("changed modeled-memset thunk refuses", True))
    else:
        checks.append(("changed modeled-memset thunk refuses", False))
    zero_fill_source = bytearray(synthetic)
    zero_callsite = 0x80010100
    zero_return = 0x80010108
    zero_target = 0x80010140
    zero_next = 0x80010180
    zero_destination = 0x80010200
    zero_callsite_code = pack_words((0x0C004050, 0x24050008))
    zero_return_code = pack_words((0x0C004060, 0x26040038))
    zero_fill_source[0x800 + 0x100 : 0x800 + 0x108] = zero_callsite_code
    zero_fill_source[0x800 + 0x108 : 0x800 + 0x110] = zero_return_code
    zero_fill_source[0x800 + 0x140 : 0x800 + 0x140 + len(ZERO_FILL_BODY)] = (
        ZERO_FILL_BODY
    )
    observed_zero = ObservedZeroFill(
        callsite=zero_callsite,
        expected_callsite=zero_callsite_code,
        target=zero_target,
        expected_body=ZERO_FILL_BODY,
        return_pc=zero_return,
        expected_return=zero_return_code,
        destination=zero_destination,
        words=8,
        next_target=zero_next,
        next_ra=zero_return + 8,
        next_a0=0x38,
    )
    observed_replay = build_replay(
        bytes(zero_fill_source),
        resume_target=0x80010000 + prefix_offset,
        registers=replay_registers,
        lo=0,
        hi=0,
        expected_prefix=RESIDENT_PREFIX,
        observed_zero_fill=observed_zero,
    )
    observed_payload = observed_replay.data[0x800:]
    checks.append(
        (
            "observed zero-fill redirects both checked caller slices and keeps the retail body",
            observed_payload[0x100:0x104] != zero_callsite_code[:4]
            and observed_payload[0x108:0x110] != zero_return_code
            and observed_payload[0x140 : 0x140 + len(ZERO_FILL_BODY)] == ZERO_FILL_BODY,
        )
    )
    changed_zero_fill = bytearray(zero_fill_source)
    changed_zero_fill[0x800 + 0x140] ^= 1
    try:
        build_replay(
            bytes(changed_zero_fill),
            resume_target=0x80010000 + prefix_offset,
            registers=replay_registers,
            lo=0,
            hi=0,
            expected_prefix=RESIDENT_PREFIX,
            observed_zero_fill=observed_zero,
        )
    except ReplayRefusal:
        checks.append(("changed observed zero-fill body refuses", True))
    else:
        checks.append(("changed observed zero-fill body refuses", False))
    for label, passed in checks:
        print(f"  {'PASS' if passed else 'FAIL'} {label}")
    failed = sum(not passed for _, passed in checks)
    print(f"compare_crt0_trace --selftest: {len(checks) - failed}/{len(checks)} passed")
    return 1 if failed else 0
