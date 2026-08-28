#!/usr/bin/env python3
"""Emit CTR's resident substrate from the already-provisioned, identity-checked executable."""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from extract_overlays import OVERLAY_DIR, extract, seed_stems
from provision import CTR_USA, ROOT, ProvisionError, verify_executable

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
    command = [
        sys.executable,
        str(EMITTER),
        str(arguments.exe),
        str(OUTPUT),
        "--seeds",
        str(SEEDS),
    ]
    # CTR's overlays live inside BIGFILE.BIG, so the emitter's overlay input has to be produced from
    # the selected disc. Declaring a base and emitting without the image would silently drop every
    # overlay function and fail much later as an unexplained recomp-MISS, so this refuses instead.
    stems = seed_stems(SEEDS)
    if stems:
        extract(None, None)
        missing = sorted(
            stem for stem in stems if not (OVERLAY_DIR / f"{stem}.BIN").is_file()
        )
        if missing:
            raise ProvisionError(
                f"overlay extraction produced no image for {', '.join(missing)}"
            )
        command += ["--overlays", str(OVERLAY_DIR)]
    result = subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        check=False,
    )
    if result.returncode:
        raise ProvisionError(
            f"shipping recompiler failed with exit {result.returncode}"
        )
    print(f"[emit] verified executable: {arguments.exe}")
    print(f"[emit] generated substrate: {OUTPUT.parent.relative_to(ROOT)}/")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ProvisionError) as error:
        print(f"[emit] REFUSED: {error}", file=sys.stderr)
        sys.exit(2)
