#!/usr/bin/env python3
"""Exercise CTR's shipping frame driver across finite and zero-progress budget exits."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def run(binary: Path, mode: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(binary), mode], capture_output=True, text=True, timeout=30, check=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    finite = run(args.binary, "finite")
    if finite.returncode != 0 or "CTR finite budget field: PASS" not in finite.stdout:
        print(f"finite field failed (exit {finite.returncode}):\n{finite.stdout}{finite.stderr}")
        return 1
    stalled = run(args.binary, "stalled")
    if stalled.returncode == 0 or "cannot resume budget exit" not in stalled.stderr or "cycles=0" not in stalled.stderr:
        print(f"zero-cycle refusal failed (exit {stalled.returncode}):\n{stalled.stdout}{stalled.stderr}")
        return 1
    print(finite.stdout, end="")
    print("CTR zero-cycle host loop: refused with cycles=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
