#!/usr/bin/env python3
"""Emit CTR's resident substrate from the already-provisioned, identity-checked executable."""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from provision import CTR_USA, ProvisionError, ROOT, verify_executable


DEFAULT_EXE = ROOT / "scratch" / "raw" / "ctr" / CTR_USA.name
OUTPUT = ROOT / "generated" / "recompiled.c"
SEEDS = ROOT / "game" / "recomp_seeds.json"
EMITTER = ROOT / "external" / "psxport" / "tools" / "recomp" / "emit.py"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=pathlib.Path, default=DEFAULT_EXE)
    parser.add_argument("--shards", type=int, default=8)
    arguments = parser.parse_args()
    if arguments.shards <= 0:
        raise ProvisionError("--shards must be positive")
    if not arguments.exe.is_file():
        raise ProvisionError(
            f"verified executable is missing at {arguments.exe}; run tools/provision.py first"
        )
    verify_executable(arguments.exe)
    if not EMITTER.is_file() or not SEEDS.is_file():
        raise ProvisionError("shipping emitter or CTR seed manifest is missing")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["PSXPORT_SHARDS"] = str(arguments.shards)
    result = subprocess.run(
        [sys.executable, str(EMITTER), str(arguments.exe), str(OUTPUT), "--seeds", str(SEEDS)],
        cwd=ROOT,
        env=environment,
        check=False,
    )
    if result.returncode:
        raise ProvisionError(f"shipping recompiler failed with exit {result.returncode}")
    print(f"[emit] verified executable: {arguments.exe}")
    print(f"[emit] generated substrate: {OUTPUT.parent.relative_to(ROOT)}/")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ProvisionError) as error:
        print(f"[emit] REFUSED: {error}", file=sys.stderr)
        sys.exit(2)
