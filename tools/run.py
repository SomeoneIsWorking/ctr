#!/usr/bin/env python3
"""Provision, build, and launch the current Crash Team Racing port product."""

from __future__ import annotations

import argparse
import hashlib
import os
import platform
import runpy
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

from tools.provision import CTR_USA, OUTPUT_DIR

ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = ROOT / "scratch" / "build" / "player"
PORT = ROOT / "scratch" / "bin" / "ctr_port"
EXECUTABLE = OUTPUT_DIR / CTR_USA.name

PACKAGE_NAMES = {
    "cmake": {
        "fedora": "cmake",
        "debian": "cmake",
        "macos": "cmake",
        "windows": "Kitware.CMake",
    },
    "compiler": {
        "fedora": "gcc gcc-c++",
        "debian": "build-essential",
        "macos": "xcode-select --install",
        "windows": "LLVM.LLVM",
    },
    "git": {"fedora": "git", "debian": "git", "macos": "git", "windows": "Git.Git"},
    "pkg-config": {
        "fedora": "pkgconf-pkg-config",
        "debian": "pkg-config",
        "macos": "pkg-config",
        "windows": "pkgconf",
    },
    "glslc": {
        "fedora": "glslc",
        "debian": "glslc",
        "macos": "shaderc",
        "windows": "KhronosGroup.VulkanSDK",
    },
    "sdl3": {
        "fedora": "SDL3-devel",
        "debian": "libsdl3-dev",
        "macos": "sdl3",
        "windows": "sdl3:x64-windows",
    },
    "sdl3-image": {
        "fedora": "SDL3_image-devel",
        "debian": "libsdl3-image-dev",
        "macos": "sdl3_image",
        "windows": "sdl3-image:x64-windows",
    },
    "freetype2": {
        "fedora": "freetype-devel",
        "debian": "libfreetype-dev",
        "macos": "freetype",
        "windows": "freetype:x64-windows",
    },
    "zlib": {
        "fedora": "zlib-devel",
        "debian": "zlib1g-dev",
        "macos": "zlib",
        "windows": "zlib:x64-windows",
    },
    "libzstd": {
        "fedora": "libzstd-devel",
        "debian": "libzstd-dev",
        "macos": "zstd",
        "windows": "zstd:x64-windows",
    },
}

PKG_CONFIG_DEPENDENCIES = (
    ("sdl3", "SDL3 development files"),
    ("sdl3-image", "SDL3_image development files"),
    ("freetype2", "FreeType development files"),
    ("zlib", "zlib development files"),
    ("libzstd", "zstd development files"),
)


class Refusal(RuntimeError):
    """A user-facing refusal that names the required correction."""


class Host:
    """Injectable host boundary for dependency and compiler capability probes."""

    @staticmethod
    def which(name: str) -> str | None:
        return shutil.which(name)

    @staticmethod
    def run(args: Sequence[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        kwargs.pop("check", None)
        return subprocess.run([str(value) for value in args], check=False, **kwargs)

    @staticmethod
    def system() -> str:
        return platform.system()

    @staticmethod
    def linux_distribution() -> str:
        try:
            values = {}
            for line in (
                Path("/etc/os-release")
                .read_text(encoding="utf-8", errors="replace")
                .splitlines()
            ):
                key, separator, value = line.partition("=")
                if separator:
                    values[key] = value.strip().strip('"').lower()
        except OSError:
            return "unknown"
        return " ".join((values.get("ID", ""), values.get("ID_LIKE", ""))).strip()


def say(message: str) -> None:
    print(f"[run] {message}", file=sys.stderr)


def host_family(host: Host) -> str:
    system = host.system()
    if system == "Darwin":
        return "macos"
    if system == "Windows":
        return "windows"
    if system == "Linux":
        distribution = set(host.linux_distribution().split())
        if distribution & {"fedora", "rhel", "centos", "rocky", "almalinux"}:
            return "fedora"
        if distribution & {"debian", "ubuntu", "linuxmint", "pop"}:
            return "debian"
    return "unknown"


def install_instruction(host: Host, dependency: str) -> str:
    family = host_family(host)
    package = PACKAGE_NAMES[dependency].get(family)
    if package is None:
        return (
            f"install the native package providing {dependency}; no package mapping is recorded "
            f"for {host.system()}/{host.linux_distribution()}, so report that platform/version rather than guessing"
        )
    if family == "fedora":
        return f"please run: sudo dnf install {package}"
    if family == "debian":
        return f"please run: sudo apt install {package}"
    if family == "macos":
        return (
            f"please run: {package}"
            if dependency == "compiler"
            else f"please run: brew install {package}"
        )
    if dependency in {
        "sdl3",
        "sdl3-image",
        "freetype2",
        "zlib",
        "libzstd",
        "pkg-config",
    }:
        return f"please run: vcpkg install {package}"
    return f"please run: winget install {package}"


def require_tool(host: Host, name: str, dependency: str | None = None) -> str:
    resolved = host.which(name)
    if resolved is None:
        raise Refusal(
            f"required tool {name!r} was not found; {install_instruction(host, dependency or name)}"
        )
    return resolved


def require_compiler(host: Host, setting: str, language: str) -> str:
    compiler = require_tool(host, setting, "compiler")
    standard = "c11" if language == "c" else "c++20"
    try:
        probe = host.run(
            [compiler, f"-std={standard}", "-x", language, "-fsyntax-only", "-"],
            input="int main(void) { return 0; }\n",
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except OSError as error:
        raise Refusal(f"could not execute compiler {setting!r}: {error}") from error
    if probe.returncode:
        raise Refusal(
            f"compiler {setting!r} cannot compile a minimal {standard} translation unit; "
            f"{install_instruction(host, 'compiler')}"
        )
    return compiler


def require_library(host: Host, module: str, label: str) -> None:
    result = host.run(
        ["pkg-config", "--exists", module],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode:
        raise Refusal(
            f"required native library {label} ({module}) was not found; {install_instruction(host, module)}"
        )


def preflight(
    host: Host | None = None, environment: Mapping[str, str] | None = None
) -> tuple[str, str]:
    machine = host or Host()
    env = os.environ if environment is None else environment
    for tool in ("cmake", "git", "pkg-config", "glslc"):
        require_tool(machine, tool)
    cc = require_compiler(machine, env.get("CC", "cc"), "c")
    cxx = require_compiler(machine, env.get("CXX", "c++"), "c++")
    for module, label in PKG_CONFIG_DEPENDENCIES:
        require_library(machine, module, label)
    return cc, cxx


def command(
    args: Sequence[object],
    *,
    environment: Mapping[str, str] | None = None,
    quiet: bool = False,
) -> None:
    result = subprocess.run(
        [str(value) for value in args],
        cwd=ROOT,
        env=None if environment is None else dict(environment),
        stdout=subprocess.DEVNULL if quiet else None,
        check=False,
    )
    if result.returncode:
        raise Refusal(
            f"command failed ({result.returncode}): {' '.join(map(str, args))}"
        )


def toolchain_build(cc: str, cxx: str) -> Path:
    identity = hashlib.sha256(f"{cc}\0{cxx}".encode()).hexdigest()[:12]
    return BUILD_ROOT / identity


def sync_framework() -> Path:
    command([sys.executable, ROOT / "tools" / "psxport_sync.py", "--auto"])
    configured = os.environ.get("PSXPORT_DIR")
    psxport = Path(configured or ROOT / "external" / "psxport").resolve()
    if not (psxport / "cmake" / "psxport.cmake").is_file():
        raise Refusal(f"PSXPORT_DIR={psxport} is not a psxport checkout")
    revision = subprocess.run(
        ["git", "-C", str(psxport), "rev-parse", "--short", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "-C", str(psxport), "status", "--porcelain"],
        capture_output=True,
        text=True,
        check=False,
    ).stdout.strip()
    suffix = " +dirty" if dirty else ""
    if configured:
        say(
            f"framework: *** {psxport} *** (DEV CLONE {revision or 'unknown'}{suffix}) — NOT the recorded pin"
        )
    else:
        say(
            f"framework: external/psxport -> {psxport} @ {revision or 'unknown'}{suffix}"
        )
    return psxport


def configure(build: Path, psxport: Path, cc: str, cxx: str) -> None:
    command(
        [
            "cmake",
            "-S",
            ROOT,
            "-B",
            build,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            "-DPSXPORT_BUILD_TESTS=OFF",
            f"-DPSXPORT_DIR={psxport}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DPython3_EXECUTABLE={sys.executable}",
        ],
        quiet=True,
    )


def prepare(disc: str | None, psxport: Path, cc: str, cxx: str) -> None:
    build = toolchain_build(cc, cxx)
    configure(build, psxport, cc, cxx)
    command(
        [
            "cmake",
            "--build",
            build,
            "--target",
            "discdump",
            "-j",
            str(os.cpu_count() or 4),
        ]
    )
    discdump = build / "psxport_build" / "tools" / "discdump"
    if not os.access(discdump, os.X_OK):
        raise Refusal(f"build produced no shipping discdump at {discdump}")

    provision = [
        sys.executable,
        ROOT / "tools" / "provision.py",
        "--discdump",
        discdump,
    ]
    if disc:
        provision.append(disc)
    command(provision)
    if not EXECUTABLE.is_file():
        raise Refusal(
            f"provisioning produced no verified executable at {EXECUTABLE.relative_to(ROOT)}"
        )
    command([sys.executable, ROOT / "tools" / "emit_substrate.py"])

    # generated/rec_sources.cmake is a configure-time input, so configure again after emission.
    configure(build, psxport, cc, cxx)
    say("building ctr_port (incremental)…")
    command(
        [
            "cmake",
            "--build",
            build,
            "--target",
            "ctr_port",
            "-j",
            str(os.cpu_count() or 4),
        ]
    )
    if not os.access(PORT, os.X_OK):
        raise Refusal(f"build produced no executable at {PORT.relative_to(ROOT)}")


def launch(psxport: Path, *, headless: bool) -> None:
    policy = runpy.run_path(str(psxport / "tools/port/launch_environment.py"))
    policy_name = "agent_environment" if headless else "player_environment"
    environment = policy[policy_name](os.environ)
    # CTR's product owns the framework checkout it just configured and linked. Its matching RmlUI
    # assets must come from that same checkout; an inherited path can silently pair the executable
    # with another framework tree or leave the overlay empty.
    environment["PSXPORT_ASSET_DIR"] = str(psxport)
    if headless:
        say("launching ctr_port headlessly…")
    else:
        say("launching Crash Team Racing…")
    os.execve(PORT, [str(PORT)], environment)


def execute(
    disc: str | None,
    *,
    prepare_only: bool = False,
    headless: bool = False,
    preflight_step=preflight,
    sync_step=sync_framework,
    prepare_step=prepare,
    launch_step=launch,
) -> None:
    """Run the shipping path; injectable steps keep the launcher tests hermetic."""
    cc, cxx = preflight_step()
    psxport = sync_step()
    prepare_step(disc, psxport, cc, cxx)
    if prepare_only:
        say("Crash Team Racing is built and ready.")
        return
    launch_step(psxport, headless=headless)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "disc", nargs="?", help="CTR USA CHD; otherwise use env/.env/drop-in"
    )
    parser.add_argument(
        "--prepare-only",
        action="store_true",
        help="provision and build without launching",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="launch without a window or audio device",
    )
    return parser.parse_args(list(argv))


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        execute(
            arguments.disc,
            prepare_only=arguments.prepare_only,
            headless=arguments.headless,
        )
    except (OSError, Refusal) as error:
        print(f"[run] error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
