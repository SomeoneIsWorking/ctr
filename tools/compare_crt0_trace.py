#!/usr/bin/env python3
"""Compare CTR's independent oracle boundaries with shipping generated execution."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import subprocess
import sys


REGISTER_NAMES = (
    "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5",
    "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0",
    "k1", "gp", "sp", "fp", "ra", "lo", "hi",
)


class Refusal(RuntimeError):
    """The evidence was incomplete, so no agreement can be claimed."""


@dataclasses.dataclass(frozen=True)
class Boundary:
    target: int
    pc: int
    registers: dict[str, int]
    step: int | None = None


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
        result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=60)
    except subprocess.TimeoutExpired as error:
        raise Refusal(f"{label} exceeded the 60-second evidence window") from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise Refusal(f"{label} refused or failed with exit {result.returncode}:\n{detail}")
    return result


def parse_register_block(text: str, tag: str, label: str) -> tuple[int, dict[str, int], int | None]:
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


def parse_call_boundary(text: str, capture_tag: str, register_tag: str, label: str) -> Boundary:
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
    if capture_step is not None and register_step is not None and capture_step != register_step:
        raise Refusal(f"{label} capture step {capture_step} disagrees with register step {register_step}")
    return Boundary(int(capture.group(1), 16), pc, registers, capture_step or register_step)


def parse_boundary(text: str, port: bool) -> Boundary:
    prefix = "PORT-" if port else ""
    return parse_call_boundary(
        text,
        f"{prefix}CAPTURED-CALL",
        f"{prefix}CALL-BOUNDARY",
        "port" if port else "oracle",
    )


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
    pc, registers, block_step = parse_register_block(text, f"{prefix}MODELED-RETURN", label)
    metadata_step = int(match.group(7)) if match.group(7) is not None else None
    if metadata_step is not None and block_step is not None and metadata_step != block_step:
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


def compare_boundary(oracle: Boundary, port: Boundary, stage: str) -> list[tuple[str, int, int]]:
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
        (f"{stage}.{name}", oracle.registers[name], port.registers[name]) for name in REGISTER_NAMES
    )
    return rows


def compare(oracle: Boundary, port: Boundary) -> list[tuple[str, int, int]]:
    return [(name.removeprefix("first."), left, right) for name, left, right in compare_boundary(oracle, port, "first")]


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
    rows.extend(compare_boundary(oracle.post_return_call, port.post_return_call, "post"))
    return rows


def print_rows(rows: list[tuple[str, int, int]], post_init_heap: bool) -> int:
    differences = 0
    print("  field              oracle       generated     verdict")
    print("  " + "-" * 60)
    for name, oracle, port in rows:
        verdict = "AGREE" if oracle == port else "DISAGREE"
        differences += oracle != port
        print(f"  {name:<15}  0x{oracle:08X}   0x{port:08X}   {verdict}")
    label = "ctr04 post-InitHeap compare" if post_init_heap else "ctr04 compare"
    print(f"{label}: {len(rows) - differences}/{len(rows)} fields agree; {differences} differ")
    return differences


def format_boundary(boundary: Boundary, capture_tag: str, register_tag: str) -> str:
    step = f" step={boundary.step}" if boundary.step is not None else ""
    lines = [
        f"# {capture_tag} target=0x{boundary.target:08X} ra=0x{boundary.registers['ra']:08X}{step}",
        f"# {register_tag}-REGS{step} pc=0x{boundary.pc:08X}",
    ]
    lines.extend(f"# {register_tag}-REG {name}=0x{boundary.registers[name]:08X}" for name in REGISTER_NAMES)
    return "\n".join(lines)


def format_evidence(evidence: PostInitHeapEvidence, port: bool) -> str:
    prefix = "PORT-" if port else ""
    model = evidence.modeled_return
    step = f" step={model.step}" if model.step is not None else ""
    model_lines = [
        f"# {prefix}MODELED-BIOS-RETURN table={model.table} function=0x{model.function:02X} "
        f"target=0x{model.target:08X} ra=0x{model.return_pc:08X} v0=0x{model.v0:08X} "
        f"v1=0x{model.v1:08X}{step}",
        f"# {prefix}MODELED-RETURN-REGS{step} pc=0x{model.boundary.pc:08X}",
    ]
    model_lines.extend(
        f"# {prefix}MODELED-RETURN-REG {name}=0x{model.boundary.registers[name]:08X}"
        for name in REGISTER_NAMES
    )
    return "\n".join(
        (
            format_boundary(evidence.first_call, f"{prefix}CAPTURED-CALL", f"{prefix}CALL-BOUNDARY"),
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
    modeled_boundary = Boundary(0x80001008, 0x80001008, {**registers, "v0": 0, "t1": 0x39, "t2": 0xA0}, 103)
    model = ModeledReturn("A", 0x39, 0xA0, 0x80001008, 0, registers["v1"], modeled_boundary, 103)
    post = Boundary(0x80002000, 0x80002000, {**modeled_boundary.registers, "ra": 0x80001020}, 108)
    expected = PostInitHeapEvidence(first, model, post)
    oracle = parse_post_init_heap(format_evidence(expected, port=False), port=False)
    port = parse_post_init_heap(format_evidence(dataclasses.replace(expected, first_call=dataclasses.replace(first, step=None), modeled_return=dataclasses.replace(model, boundary=dataclasses.replace(modeled_boundary, step=None), step=None), post_return_call=dataclasses.replace(post, step=None)), port=True), port=True)
    rows = compare_post_init_heap(oracle, port)
    forced = dataclasses.replace(
        port,
        post_return_call=dataclasses.replace(
            port.post_return_call,
            registers={**port.post_return_call.registers, "gp": 0},
        ),
    )
    checks = [
        ("complete three-boundary evidence parses", oracle == expected),
        ("equal oracle and generated evidence", not any(left != right for _, left, right in rows)),
        (
            "forced opposite is visible",
            sum(left != right for _, left, right in compare_post_init_heap(oracle, forced)) == 1,
        ),
        ("repeat-run nondeterminism is visible", oracle != dataclasses.replace(oracle, post_return_call=dataclasses.replace(post, step=109))),
    ]
    try:
        parse_post_init_heap(format_boundary(first, "CAPTURED-CALL", "CALL-BOUNDARY"), port=False)
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
    for label, passed in checks:
        print(f"  {'PASS' if passed else 'FAIL'} {label}")
    failed = sum(not passed for _, passed in checks)
    print(f"compare_crt0_trace --selftest: {len(checks) - failed}/{len(checks)} passed")
    return 1 if failed else 0


def parse_forced_field(text: str, default_stage: str) -> tuple[str, str, int]:
    field, separator, raw_value = text.partition("=")
    if not separator:
        raise Refusal("--force-port-field must be [STAGE:]NAME=VALUE")
    if ":" in field:
        stage, name = field.split(":", 1)
    else:
        stage, name = default_stage, field
    if stage not in {"first", "modeled", "post"} or name not in REGISTER_NAMES:
        raise Refusal("--force-port-field must name first, modeled, or post and one boundary register")
    try:
        return stage, name, int(raw_value, 0)
    except ValueError as error:
        raise Refusal(f"invalid forced value {raw_value!r}") from error


def force_field(evidence: PostInitHeapEvidence, stage: str, name: str, value: int) -> PostInitHeapEvidence:
    if stage == "first":
        return dataclasses.replace(
            evidence,
            first_call=dataclasses.replace(
                evidence.first_call, registers={**evidence.first_call.registers, name: value}
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", nargs="?")
    parser.add_argument("--oracle-trace")
    parser.add_argument("--port-trace")
    parser.add_argument("--steps", type=int, default=400000)
    parser.add_argument("--post-init-heap", action="store_true")
    parser.add_argument("--force-port-field", help="test-only post-capture mutation, [STAGE:]NAME=VALUE")
    parser.add_argument("--expect-difference", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()

    if arguments.selftest:
        if arguments.exe or arguments.oracle_trace or arguments.port_trace:
            raise Refusal("--selftest takes no executable or trace tools")
        return selftest()
    if not arguments.exe or not arguments.oracle_trace or not arguments.port_trace:
        raise Refusal("an executable, --oracle-trace, and --port-trace are all required")
    if arguments.steps <= 0:
        raise Refusal("--steps must be positive; an empty run is not agreement")

    exe = pathlib.Path(arguments.exe)
    oracle_tool = pathlib.Path(arguments.oracle_trace)
    port_tool = pathlib.Path(arguments.port_trace)
    for path, label in ((exe, "executable"), (oracle_tool, "oracle tracer"), (port_tool, "port tracer")):
        if not path.is_file():
            raise Refusal(f"{label} does not exist: {path}")

    scratch = pathlib.Path(__file__).resolve().parents[1] / "scratch" / "logs"
    scratch.mkdir(parents=True, exist_ok=True)
    oracle_output = scratch / "ctr04-oracle-boundary.trace"
    base_oracle_command = [
        str(oracle_tool), str(exe), "--steps", str(arguments.steps), "--capture-call", "1",
        "--summary-only",
    ]

    if arguments.post_init_heap:
        oracle_repeat_output = scratch / "ctr04-oracle-boundary-repeat.trace"
        modeled_command = [*base_oracle_command, "--model-bios-return", "A:0x39:0"]
        run([*modeled_command, "--out", str(oracle_output)], "oracle post-InitHeap trace A")
        run([*modeled_command, "--out", str(oracle_repeat_output)], "oracle post-InitHeap trace B")
        oracle = parse_post_init_heap(oracle_output.read_text(encoding="utf-8"), port=False)
        oracle_repeat = parse_post_init_heap(oracle_repeat_output.read_text(encoding="utf-8"), port=False)
        if oracle != oracle_repeat:
            raise Refusal("two independent oracle runs produced different boundary state or step counts")
        print("ctr04 post-InitHeap compare: determinism PASS — two oracle runs produced identical three-boundary evidence")

        port_result = run(
            [
                str(port_tool), str(exe), "--target", f"0x{oracle.first_call.target:08X}",
                "--model-init-heap-return", "--post-target", f"0x{oracle.post_return_call.target:08X}",
            ],
            "generated post-InitHeap port trace",
        )
        port = parse_post_init_heap(port_result.stdout, port=True)
        if arguments.force_port_field:
            stage, name, value = parse_forced_field(arguments.force_port_field, "post")
            port = force_field(port, stage, name, value)
            print(f"ctr04 post-InitHeap compare: TEST-ONLY forced generated {stage}.{name}=0x{value:08X}")
        rows = compare_post_init_heap(oracle, port)
    else:
        run([*base_oracle_command, "--out", str(oracle_output)], "oracle trace")
        oracle_boundary = parse_boundary(oracle_output.read_text(encoding="utf-8"), port=False)
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

    differences = print_rows(rows, arguments.post_init_heap)
    if arguments.expect_difference:
        if differences == 0:
            raise Refusal("--expect-difference was requested but the comparator reported agreement")
        print("ctr04 compare: PASS — the forced opposite produced a named disagreement")
        return 0
    return 1 if differences else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Refusal as error:
        print(f"compare_crt0_trace: REFUSING — {error}", file=sys.stderr)
        print("Nothing was compared. This is not agreement.", file=sys.stderr)
        sys.exit(2)
