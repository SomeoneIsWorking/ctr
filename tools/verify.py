#!/usr/bin/env python3
"""Run CTR's canonical asset-free native/Lightrec product verification."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PSXPORT = ROOT / "external" / "psxport"


def main() -> int:
    resolved = subprocess.run(
        [sys.executable, ROOT / "tools" / "psxport_sync.py", "--auto"],
        cwd=ROOT,
        check=False,
    )
    if resolved.returncode:
        return resolved.returncode
    sys.path.insert(0, str(PSXPORT / "tools"))
    from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification

    build = ROOT / "build" / "agent-clang"
    return run_consumer_verification(
        ConsumerVerifyConfig(
            name="Crash Team Racing",
            root=ROOT,
            build=build,
            psxport=PSXPORT,
            product=build / "ctr_port",
            cmake_module=ROOT / "CMakeLists.txt",
            test_regex=r"^ctr_",
            cmake_definitions=("-DBUILD_TESTING=ON", "-DPSXPORT_BUILD_TESTS=OFF"),
            build_targets=("cpp_policy", "provision_selftest", "overlay_extract_selftest"),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
