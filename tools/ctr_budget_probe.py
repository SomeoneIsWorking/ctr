#!/usr/bin/env python3
"""Observe one CTR Lightrec budget continuation without changing the shipping executor."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "scratch" / "gdb" / "ctr_budget_probe.generated.gdb"
LOG = ROOT / "scratch" / "gdb" / "ctr_budget_probe.log"
GDB_MODULE = ROOT / "tools" / "ctr_budget_probe_gdb.py"
PRODUCT = ROOT / "build" / "agent-clang" / "ctr_port"
FIXTURE = ROOT / "build" / "agent-clang" / "ctr_budget_probe_fixture"
PROBE_TIMEOUT_SECONDS = 300
TERMINATION_GRACE_SECONDS = 3


def stop_owned_probe(process: subprocess.Popen[str]) -> tuple[str, str]:
    """Stop only the GDB process group created for this invocation."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        return process.communicate(timeout=TERMINATION_GRACE_SECONDS)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        return process.communicate()


def gdb_script(mode: str) -> str:
    location = "game/core/frame_driver.cpp:172" if mode == "product" else "ctr::budgetProbeFixtureStop"
    runtime = "runtime_" if mode == "product" else "runtime"
    return f"""set pagination off
set confirm off
set debuginfod enabled off
set max-value-size unlimited
python probe_mode = {mode!r}
python exec(compile(open({str(GDB_MODULE)!r}, encoding='utf-8').read(), {str(GDB_MODULE)!r}, 'exec'))
break {location}
commands
  silent
  python admit()
  if $ctr_probe_ready
    python arm()
    print {runtime}.dispatch(core, core.pc)
    python finish()
  else
    printf "CTR_PROBE_UNREACHED scanned=0 retained=0; no continuation dispatched\\n"
  end
  quit
end
run
"""


def run_probe(binary: Path, mode: str, argument: str | None = None) -> dict[str, object]:
    if not sys.platform.startswith("linux"):
        raise RuntimeError("this GDB probe requires Linux process-group ownership")
    if not binary.is_file():
        raise RuntimeError(f"Clang-built diagnostic target is missing: {binary}")
    gdb = shutil.which("gdb")
    if not gdb:
        raise RuntimeError("gdb is required for this diagnostic; on Fedora, please run: sudo dnf install gdb")
    GENERATED.parent.mkdir(parents=True, exist_ok=True)
    GENERATED.write_text(gdb_script(mode), encoding="utf-8")
    command = [gdb, "-q", "-batch", "-x", str(GENERATED), "--args", str(binary)]
    if argument:
        command.append(argument)
    environment = os.environ.copy()
    if mode == "product":
        environment["PSXPORT_VK_HEADLESS"] = "1"
        environment["PSXPORT_NOAUDIO"] = "1"
        environment["PSXPORT_ASSET_DIR"] = str(ROOT / "external" / "psxport")
        # GDB stops guest execution between block entries, so frame-wall-time is not progress.
        environment["PSXPORT_WATCHDOG"] = "0"
        environment.pop("PSXPORT_NOPACE", None)
        print("diagnostic scope: host frame-progress watchdog disabled during GDB pauses; guest spin detection and normal pacing retained")
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=PROBE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        stdout, stderr = stop_owned_probe(process)
        LOG.write_text(stdout + stderr, encoding="utf-8")
        raise RuntimeError(f"GDB exceeded {PROBE_TIMEOUT_SECONDS}s; stopped its owned process group; see {LOG}") from None
    LOG.write_text(stdout + stderr, encoding="utf-8")
    admissions = [line.removeprefix("CTR_PROBE_ADMISSION ") for line in stdout.splitlines() if line.startswith("CTR_PROBE_ADMISSION ")]
    results = [line.removeprefix("CTR_PROBE_RESULT ") for line in stdout.splitlines() if line.startswith("CTR_PROBE_RESULT ")]
    if process.returncode or len(admissions) != 1 or len(results) > 1:
        raise RuntimeError(
            f"GDB probe did not reach one valid stop (exit {process.returncode}, admissions {len(admissions)}, results {len(results)}); see {LOG}"
        )
    admission = json.loads(admissions[0])
    print("admission: " + json.dumps(admission, sort_keys=True))
    if admission["status"] == "refused":
        if results:
            raise RuntimeError("refused probe nevertheless dispatched a continuation")
        return {"status": "refused", "admission": admission}
    if len(results) != 1:
        raise RuntimeError(f"admitted probe failed to report its one continuation; see {LOG}")
    result = json.loads(results[0])
    print("result: " + json.dumps(result, sort_keys=True))
    if result["block_entries_scanned"] == 0 or result["target_hits"]["0x8006A57C"] == 0 or result["negative_hits"] != 0:
        raise RuntimeError(f"probe controls failed; see {LOG}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--selftest", action="store_true", help="run synthetic progress and fail-closed controls")
    selection.add_argument("--product", action="store_true", help="run one retail CTR budget probe (requires configured user media)")
    parser.add_argument("--binary", type=Path, help="Clang-built fixture or product executable; defaults to build/agent-clang")
    arguments = parser.parse_args()
    try:
        if arguments.selftest:
            expected = {"advance": "progress", "stagnant": "net-unchanged", "missing-image": "refused"}
            for mode, status in expected.items():
                result = run_probe(arguments.binary or FIXTURE, mode, mode)
                if result["status"] != status:
                    raise RuntimeError(f"{mode} control produced {result['status']}, expected {status}")
            try:
                run_probe(arguments.binary or FIXTURE, "missing-trigger", "missing-trigger")
            except RuntimeError as error:
                if "admissions 0" not in str(error):
                    raise
            else:
                raise RuntimeError("missing-trigger control passed without an admitted stop")
            print("CTR budget probe synthetic controls: 4/4 PASS")
        else:
            result = run_probe(arguments.binary or PRODUCT, "product")
            if result["status"] == "refused":
                return 1
        return 0
    except RuntimeError as error:
        print(f"CTR budget probe: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
