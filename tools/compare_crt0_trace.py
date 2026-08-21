#!/usr/bin/env python3
"""Compare CTR's independent oracle crt0 boundary with shipping generated execution."""

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


def run(command: list[str], label: str) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=60)
    except subprocess.TimeoutExpired as error:
        raise Refusal(f"{label} exceeded the 60-second evidence window") from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise Refusal(f"{label} refused or failed with exit {result.returncode}:\n{detail}")
    return result


def parse_boundary(text: str, port: bool) -> Boundary:
    prefix = "PORT-" if port else ""
    capture = re.search(rf"^# {prefix}CAPTURED-CALL target=0x([0-9A-Fa-f]+)\b", text, re.MULTILINE)
    header = re.search(
        rf"^# {prefix}CALL-BOUNDARY-REGS(?: step=\d+)? pc=0x([0-9A-Fa-f]+)\s*$",
        text,
        re.MULTILINE,
    )
    register_pattern = re.compile(
        rf"^# {prefix}CALL-BOUNDARY-REG (\w+)=0x([0-9A-Fa-f]+)\s*$", re.MULTILINE
    )
    if capture is None or header is None:
        raise Refusal(f"{'port' if port else 'oracle'} output has no complete first-call boundary")
    registers = {name: int(value, 16) for name, value in register_pattern.findall(text)}
    missing = sorted(set(REGISTER_NAMES) - registers.keys())
    extra = sorted(registers.keys() - set(REGISTER_NAMES))
    if missing or extra:
        raise Refusal(
            f"{'port' if port else 'oracle'} boundary register coverage changed "
            f"(missing={missing or 'none'}, extra={extra or 'none'})"
        )
    return Boundary(int(capture.group(1), 16), int(header.group(1), 16), registers)


def compare(oracle: Boundary, port: Boundary) -> list[tuple[str, int, int]]:
    if oracle.target != oracle.pc:
        raise Refusal(
            f"oracle captured target 0x{oracle.target:08X} but its boundary PC is 0x{oracle.pc:08X}"
        )
    if port.target != oracle.target:
        raise Refusal(
            f"port claims target 0x{port.target:08X}, not oracle target 0x{oracle.target:08X}"
        )
    rows = [("pc", oracle.pc, port.pc)]
    rows.extend((name, oracle.registers[name], port.registers[name]) for name in REGISTER_NAMES)
    return rows


def print_rows(rows: list[tuple[str, int, int]]) -> int:
    differences = 0
    print("  field         oracle       generated     verdict")
    print("  " + "-" * 55)
    for name, oracle, port in rows:
        verdict = "AGREE" if oracle == port else "DISAGREE"
        differences += oracle != port
        print(f"  {name:<10}  0x{oracle:08X}   0x{port:08X}   {verdict}")
    print(f"ctr04 compare: {len(rows) - differences}/{len(rows)} fields agree; {differences} differ")
    return differences


def selftest() -> int:
    registers = {name: index + 1 for index, name in enumerate(REGISTER_NAMES)}
    boundary = Boundary(0x80001000, 0x80001000, registers)
    checks = [
        ("equal boundaries", not any(left != right for _, left, right in compare(boundary, boundary))),
        (
            "forced opposite is visible",
            sum(left != right for _, left, right in compare(
                boundary, dataclasses.replace(boundary, registers={**registers, "gp": 0})
            )) == 1,
        ),
    ]
    try:
        parse_boundary("# FIRST CALL WAS NOT REACHED", port=False)
    except Refusal:
        checks.append(("missing boundary refuses", True))
    else:
        checks.append(("missing boundary refuses", False))
    for label, passed in checks:
        print(f"  {'PASS' if passed else 'FAIL'} {label}")
    failed = sum(not passed for _, passed in checks)
    print(f"compare_crt0_trace --selftest: {len(checks) - failed}/{len(checks)} passed")
    return 1 if failed else 0


def parse_forced_field(text: str) -> tuple[str, int]:
    name, separator, raw_value = text.partition("=")
    if not separator or name not in REGISTER_NAMES:
        raise Refusal("--force-port-field must be one named boundary register, for example gp=0")
    try:
        return name, int(raw_value, 0)
    except ValueError as error:
        raise Refusal(f"invalid forced value {raw_value!r}") from error


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", nargs="?")
    parser.add_argument("--oracle-trace")
    parser.add_argument("--port-trace")
    parser.add_argument("--steps", type=int, default=400000)
    parser.add_argument("--force-port-field", help="test-only post-capture mutation, NAME=VALUE")
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
    run(
        [
            str(oracle_tool), str(exe), "--steps", str(arguments.steps), "--capture-first-call",
            "--summary-only", "--out", str(oracle_output),
        ],
        "oracle trace",
    )
    oracle = parse_boundary(oracle_output.read_text(encoding="utf-8"), port=False)
    port_result = run(
        [str(port_tool), str(exe), "--target", f"0x{oracle.target:08X}"], "generated port trace"
    )
    port = parse_boundary(port_result.stdout, port=True)

    if arguments.force_port_field:
        name, value = parse_forced_field(arguments.force_port_field)
        port = dataclasses.replace(port, registers={**port.registers, name: value})
        print(f"ctr04 compare: TEST-ONLY forced generated {name}=0x{value:08X}")

    differences = print_rows(compare(oracle, port))
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
