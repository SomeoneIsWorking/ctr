#!/usr/bin/env python3
"""Resolve the CTR USA disc, extract SCUS_944.26, and verify its measured identity."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "scratch" / "raw" / "ctr"
DISC_ENV = "PSXPORT_CTR_DISC"
GENERIC_DISC_ENV = "PSXPORT_DISC"
EXE_ON_DISC = "SCUS_944.26"
SYSTEM_CNF = "SYSTEM.CNF"


class ProvisionError(RuntimeError):
    """A refusal with enough context for the operator to correct the input."""


@dataclass(frozen=True)
class ExecutableIdentity:
    name: str
    file_size: int
    sha256: str
    entry: int
    load_address: int
    text_size: int
    stack: int


CTR_USA = ExecutableIdentity(
    name=EXE_ON_DISC,
    file_size=0x7E000,
    sha256="7b4aac0bf2f6310984e599295df17b457da5a23b270c20200cefef6079efb838",
    entry=0x8007793C,
    load_address=0x80010000,
    text_size=0x0007D800,
    stack=0x801FFFF0,
)


def dotenv_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.is_file():
        return values
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        if key not in values:
            values[key] = value
    return values


def rooted_path(value: str, root: Path) -> Path:
    path = Path(value).expanduser()
    return path if path.is_absolute() else root / path


def resolve_disc(
    argument: str | None,
    *,
    root: Path = ROOT,
    environ: Mapping[str, str] = os.environ,
) -> tuple[Path, str]:
    candidates: list[tuple[str, str]] = []
    if argument:
        candidates.append((argument, "command line"))
    else:
        for key in (DISC_ENV, GENERIC_DISC_ENV):
            if environ.get(key):
                candidates.append((environ[key], f"${key}"))
                break
        if not candidates:
            values = dotenv_values(root / ".env")
            for key in (DISC_ENV, GENERIC_DISC_ENV):
                if values.get(key):
                    candidates.append((values[key], f".env:{key}"))
                    break
        if not candidates:
            dropins = sorted(
                (path for path in root.iterdir() if path.is_file() and path.suffix.casefold() == ".chd"),
                key=lambda path: path.name.casefold(),
            )
            if dropins:
                candidates.append((str(dropins[0]), "sorted root *.chd drop-in"))

    if not candidates:
        raise ProvisionError(
            f"no disc image: pass a path, set {DISC_ENV} or {GENERIC_DISC_ENV}, "
            "add one of those keys to .env, or place a *.chd in the repository root"
        )
    value, source = candidates[0]
    path = rooted_path(value, root)
    if not path.is_file():
        raise ProvisionError(f"{source} selected a missing disc image: {path}")
    return path.resolve(), source


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_executable(path: Path, expected: ExecutableIdentity = CTR_USA) -> None:
    data = path.read_bytes()
    mismatches: list[str] = []
    if len(data) != expected.file_size:
        mismatches.append(f"file size {len(data):#x}, expected {expected.file_size:#x}")
    digest = sha256(data)
    if digest != expected.sha256:
        mismatches.append(f"SHA-256 {digest}, expected {expected.sha256}")
    if len(data) < 0x34:
        mismatches.append("file is too short for a PS-X EXE header")
    else:
        if data[:8] != b"PS-X EXE":
            mismatches.append(f"magic {data[:8]!r}, expected b'PS-X EXE'")
        fields = (
            ("entry", 0x10, expected.entry),
            ("load address", 0x18, expected.load_address),
            ("text size", 0x1C, expected.text_size),
            ("stack", 0x30, expected.stack),
        )
        for label, offset, wanted in fields:
            actual = struct.unpack_from("<I", data, offset)[0]
            if actual != wanted:
                mismatches.append(f"{label} {actual:#010x}, expected {wanted:#010x}")
    if mismatches:
        raise ProvisionError(f"{path.name} is not the measured CTR USA executable: " + "; ".join(mismatches))


def verify_system_cnf(path: Path) -> None:
    text = path.read_bytes().decode("ascii", errors="replace")
    boot = re.search(r"(?im)^\s*BOOT\s*=\s*cdrom:\\([^\r\n]+)\s*$", text)
    if not boot:
        raise ProvisionError("SYSTEM.CNF has no BOOT=cdrom:\\... entry")
    target = boot.group(1).strip().upper()
    if target != f"{EXE_ON_DISC};1":
        raise ProvisionError(f"SYSTEM.CNF boots {target}, expected {EXE_ON_DISC};1")


def find_discdump(override: str | None, environ: Mapping[str, str] = os.environ) -> Path:
    explicit = override or environ.get("PSXPORT_DISCDUMP")
    if explicit:
        candidate = rooted_path(explicit, ROOT)
        if not candidate.is_file() or not os.access(candidate, os.X_OK):
            raise ProvisionError(f"explicit discdump is not executable: {candidate}")
        return candidate.resolve()
    candidate = ROOT / "build" / "psxport_build" / "tools" / "discdump"
    if candidate.is_file() and os.access(candidate, os.X_OK):
        return candidate
    raise ProvisionError(
        f"shipping discdump is missing at {candidate}; run "
        "CCACHE_DISABLE=1 cmake --build build --target discdump"
    )


def run_extract(discdump: Path, disc: Path, name: str, output_dir: Path) -> None:
    result = subprocess.run(
        [str(discdump), "get", name, str(disc), str(output_dir)],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or "no diagnostic"
        raise ProvisionError(f"discdump could not extract {name}: {detail}")


def cleanup_staging(staging: Path) -> None:
    if not staging.exists():
        return
    for child in staging.iterdir():
        if not child.is_file():
            raise ProvisionError(f"refusing to clean unexpected staging entry: {child}")
        child.unlink()
    staging.rmdir()


def provision(disc_argument: str | None, discdump_override: str | None) -> Path:
    disc, source = resolve_disc(disc_argument)
    discdump = find_discdump(discdump_override)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    staging = OUTPUT_DIR / f"provision-staging-{os.getpid()}"
    if staging.exists():
        raise ProvisionError(f"refusing pre-existing staging directory: {staging}")
    staging.mkdir()
    try:
        run_extract(discdump, disc, SYSTEM_CNF, staging)
        run_extract(discdump, disc, EXE_ON_DISC, staging)
        staged_cnf = staging / SYSTEM_CNF
        staged_exe = staging / EXE_ON_DISC
        verify_system_cnf(staged_cnf)
        verify_executable(staged_exe)
        os.replace(staged_cnf, OUTPUT_DIR / SYSTEM_CNF)
        os.replace(staged_exe, OUTPUT_DIR / EXE_ON_DISC)
    finally:
        cleanup_staging(staging)
    output = OUTPUT_DIR / EXE_ON_DISC
    print(f"[provision] disc: {disc} ({source})")
    print(f"[provision] executable: {output.relative_to(ROOT)}")
    print(f"[provision] SHA-256: {CTR_USA.sha256}")
    return output


def fixture_identity(data: bytes) -> ExecutableIdentity:
    return ExecutableIdentity(
        name="FIXTURE.EXE",
        file_size=len(data),
        sha256=sha256(data),
        entry=struct.unpack_from("<I", data, 0x10)[0],
        load_address=struct.unpack_from("<I", data, 0x18)[0],
        text_size=struct.unpack_from("<I", data, 0x1C)[0],
        stack=struct.unpack_from("<I", data, 0x30)[0],
    )


def selftest() -> None:
    scratch = ROOT / "scratch"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ctr-provision-selftest-", dir=scratch) as temporary:
        root = Path(temporary)
        cli = root / "cli.chd"
        game_env = root / "game-env.chd"
        generic_env = root / "generic-env.chd"
        dot_game = root / "dot-game.chd"
        dot_generic = root / "dot-generic.chd"
        drop_a = root / "a-drop.chd"
        drop_z = root / "z-drop.chd"
        for path in (cli, game_env, generic_env, dot_game, dot_generic, drop_a, drop_z):
            path.write_bytes(b"fixture")

        resolved, _ = resolve_disc(str(cli), root=root, environ={DISC_ENV: str(game_env)})
        assert resolved == cli.resolve()
        resolved, _ = resolve_disc(None, root=root, environ={DISC_ENV: str(game_env), GENERIC_DISC_ENV: str(generic_env)})
        assert resolved == game_env.resolve()
        resolved, _ = resolve_disc(None, root=root, environ={GENERIC_DISC_ENV: str(generic_env)})
        assert resolved == generic_env.resolve()
        (root / ".env").write_text(
            f"{DISC_ENV}={dot_game.name}\n{GENERIC_DISC_ENV}={dot_generic.name}\n",
            encoding="utf-8",
        )
        resolved, _ = resolve_disc(None, root=root, environ={})
        assert resolved == dot_game.resolve()
        (root / ".env").write_text(f"{GENERIC_DISC_ENV}={dot_generic.name}\n", encoding="utf-8")
        resolved, _ = resolve_disc(None, root=root, environ={})
        assert resolved == dot_generic.resolve()
        (root / ".env").unlink()
        resolved, _ = resolve_disc(None, root=root, environ={})
        assert resolved == drop_a.resolve()
        try:
            resolve_disc("missing.chd", root=root, environ={DISC_ENV: str(game_env)})
        except ProvisionError:
            pass
        else:
            raise AssertionError("an invalid explicit disc silently fell through")

        fixture = bytearray(0x900)
        fixture[:8] = b"PS-X EXE"
        struct.pack_into("<I", fixture, 0x10, 0x80012340)
        struct.pack_into("<I", fixture, 0x18, 0x80010000)
        struct.pack_into("<I", fixture, 0x1C, 0x100)
        struct.pack_into("<I", fixture, 0x30, 0x801FFFF0)
        fixture_path = root / "FIXTURE.EXE"
        fixture_path.write_bytes(fixture)
        expected = fixture_identity(fixture)
        verify_executable(fixture_path, expected)
        fixture[-1] ^= 1
        fixture_path.write_bytes(fixture)
        try:
            verify_executable(fixture_path, expected)
        except ProvisionError as error:
            assert "SHA-256" in str(error)
        else:
            raise AssertionError("mutated executable was accepted")
        cnf = root / SYSTEM_CNF
        cnf.write_text(f"BOOT = cdrom:\\{EXE_ON_DISC};1\r\n", encoding="ascii")
        verify_system_cnf(cnf)
        cnf.write_text("BOOT = cdrom:\\WRONG.EXE;1\r\n", encoding="ascii")
        try:
            verify_system_cnf(cnf)
        except ProvisionError:
            pass
        else:
            raise AssertionError("wrong SYSTEM.CNF target was accepted")

    print("[provision-selftest] PASS resolution precedence and invalid-explicit refusal")
    print("[provision-selftest] PASS executable acceptance and one-byte-mutation rejection")
    print("[provision-selftest] PASS SYSTEM.CNF acceptance and wrong-target rejection")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", nargs="?", help="CTR USA CHD; otherwise resolve env/.env/drop-in")
    parser.add_argument("--discdump", help="shipping discdump executable override")
    parser.add_argument("--verify-exe", type=Path, help="verify an already-extracted executable")
    parser.add_argument("--selftest", action="store_true", help="run hermetic both-answer tests")
    args = parser.parse_args()
    try:
        if args.selftest:
            selftest()
        elif args.verify_exe:
            verify_executable(args.verify_exe)
            print(f"[provision] verified {args.verify_exe}: {CTR_USA.sha256}")
        else:
            provision(args.disc, args.discdump)
    except (OSError, ProvisionError, AssertionError) as error:
        print(f"[provision] REFUSED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
