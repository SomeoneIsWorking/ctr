#!/usr/bin/env python3
"""Extract CTR's measured BIGFILE.BIG runtime module images.

CTR keeps its overlays inside one 270 MB archive rather than as separate disc files. The archive
index is a table of
(sector offset, byte size) pairs starting at word 2 of the file; entry N's bytes are the module the
game's loader reads to a measured base (see `titles/ctr/overlays.json`).

The title manifest is the authority for which entries matter: `BF<id>` names archive entry
`id`, so a stem is a fact about the archive, never about one run. This tool refuses rather than
skipping: a missing archive, a changed archive, an out-of-range id, or a malformed index is reported
by name, because silently omitting a runtime module corrupts the executable address space.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from provision import (
    OUTPUT_DIR,
    ROOT,
    ProvisionError,
    find_discdump,
    resolve_disc,
    run_extract,
)

BIGFILE_NAME = "BIGFILE.BIG"
BIGFILE_SIZE = 270360576
BIGFILE_SHA256 = "98779a7990f395fe3620481a1b5478149c88ff17fffa64905369f3e46368fce7"
BIGFILE_INDEX_BYTES = 3 * 2048
SECTOR_BYTES = 2048
OVERLAY_MANIFEST = ROOT / "titles" / "ctr" / "overlays.json"
OVERLAY_DIR = OUTPUT_DIR / "overlays"
STEM_PATTERN = re.compile(r"^BF(\d{4})$")
ADDRESS_PATTERN = re.compile(r"^0x[0-9A-Fa-f]{8}$")
FNV64_PATTERN = re.compile(r"^0x[0-9A-Fa-f]{16}$")

MIPS_JR_RA = 0x03E00008


@dataclass(frozen=True)
class ModuleEntry:
    stem: str
    archive_entry: int
    sector_offset: int
    byte_size: int
    load_address: int
    fnv64: int


def fnv64(image: bytes) -> int:
    """The same content-identity fold used by psxport's executable image loader."""
    value = 1469598103934665603
    for byte in image:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def read_index(index_bytes: bytes) -> list[tuple[int, int]]:
    """Parse the archive index into (sector offset, byte size) entries.

    Refuses a table that is empty, non-monotonic, or overlapping: each is a decoding error, and a
    short table read as a complete one would silently narrow every later lookup.
    """
    if len(index_bytes) < BIGFILE_INDEX_BYTES:
        raise ProvisionError(
            f"{BIGFILE_NAME} index is {len(index_bytes)} bytes; the archive reserves {BIGFILE_INDEX_BYTES}"
        )
    words = struct.unpack(f"<{len(index_bytes) // 4}I", index_bytes)
    entries: list[tuple[int, int]] = []
    position = 2
    while position + 1 < len(words):
        offset, size = words[position], words[position + 1]
        if offset == 0 and size == 0:
            break
        entries.append((offset, size))
        position += 2
    if not entries:
        raise ProvisionError(
            f"{BIGFILE_NAME} index decoded 0 entries; the archive layout is not what this tool parses"
        )
    previous_end = 0
    for index, (offset, size) in enumerate(entries):
        if offset < previous_end:
            raise ProvisionError(
                f"{BIGFILE_NAME} entry {index} starts at sector {offset}, inside entry {index - 1} "
                f"which ends at sector {previous_end}; the index is not the assumed layout"
            )
        previous_end = offset + (size + SECTOR_BYTES - 1) // SECTOR_BYTES
    return entries


def classify(image: bytes) -> tuple[bool, dict[str, int]]:
    """Report whether an image looks like MIPS code, with the counts the verdict rests on.

    Both answers are printed by the caller. "Not code" is a real and expected result — the loader
    streams data through the same arena — so this never returns a bare boolean.
    """
    words = struct.unpack(f"<{len(image) // 4}I", image[: len(image) // 4 * 4])
    counts = {
        "words": len(words),
        "jr_ra": sum(1 for word in words if word == MIPS_JR_RA),
        "jal": sum(1 for word in words if (word >> 26) == 3),
        "zero": sum(1 for word in words if word == 0),
    }
    return counts["jr_ra"] > 0 and counts["jal"] > 0, counts


def parse_module_entries(modules: object) -> dict[str, ModuleEntry]:
    """Validate the one source of runtime-module identity and physical placement."""
    if not isinstance(modules, dict):
        raise ProvisionError("overlay manifest root must be an object")
    stems: dict[str, ModuleEntry] = {}
    for stem, details in modules.items():
        if not isinstance(stem, str) or not isinstance(details, dict):
            raise ProvisionError("each overlay manifest member must map a string name to an object")
        matched = STEM_PATTERN.match(stem)
        if matched is None:
            raise ProvisionError(
                f"overlay stem {stem!r} is not BF<4-digit archive entry id>; this tool cannot tell "
                "which BIGFILE entry it names"
            )
        entry = details.get("archive_entry")
        if type(entry) is not int or entry != int(matched.group(1), 10):
            raise ProvisionError(f"overlay {stem} has inconsistent archive_entry {entry!r}")
        load_address = details.get("load_address")
        if not isinstance(load_address, str) or ADDRESS_PATTERN.fullmatch(load_address) is None:
            raise ProvisionError(f"overlay {stem} has invalid load_address {load_address!r}")
        offset = details.get("sector_offset")
        size = details.get("byte_size")
        digest = details.get("fnv64")
        if type(offset) is not int or offset < 0:
            raise ProvisionError(f"overlay {stem} has invalid sector_offset {offset!r}")
        if type(size) is not int or size < 4 or size > BIGFILE_SIZE:
            raise ProvisionError(f"overlay {stem} has invalid byte_size {size!r}")
        if not isinstance(digest, str) or FNV64_PATTERN.fullmatch(digest) is None:
            raise ProvisionError(f"overlay {stem} has invalid fnv64 {digest!r}")
        address = int(load_address, 16)
        if address & 3 or address < 0x80000000 or address + size > 0x80200000:
            raise ProvisionError(f"overlay {stem} does not fit aligned PSX main RAM")
        if offset * SECTOR_BYTES + size > BIGFILE_SIZE:
            raise ProvisionError(f"overlay {stem} extends beyond {BIGFILE_NAME}")
        stems[stem] = ModuleEntry(stem, entry, offset, size, address, int(digest, 16))
    by_address = sorted(stems.values(), key=lambda item: item.load_address)
    for previous, current in zip(by_address, by_address[1:]):
        if previous.load_address + previous.byte_size > current.load_address:
            raise ProvisionError(f"overlay {previous.stem} overlaps {current.stem} in PSX main RAM")
    return stems


def module_entries(path: Path = OVERLAY_MANIFEST) -> dict[str, ModuleEntry]:
    """Load and validate the title's runtime-module manifest."""
    return parse_module_entries(json.loads(path.read_text(encoding="utf-8")))


def provision_bigfile(disc_argument: str | None, discdump_override: str | None) -> Path:
    """Ensure the verified archive is present, extracting it from the selected disc if needed."""
    archive = OUTPUT_DIR / BIGFILE_NAME
    if archive.is_file() and archive.stat().st_size == BIGFILE_SIZE:
        return archive
    disc, source = resolve_disc(disc_argument)
    discdump = find_discdump(discdump_override)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"[overlays] extracting {BIGFILE_NAME} from {disc} ({source})")
    run_extract(discdump, disc, BIGFILE_NAME, OUTPUT_DIR)
    if not archive.is_file():
        raise ProvisionError(f"discdump produced no {BIGFILE_NAME} at {archive}")
    return archive


def verify_bigfile(archive: Path) -> None:
    size = archive.stat().st_size
    if size != BIGFILE_SIZE:
        raise ProvisionError(
            f"{archive} is {size} bytes; the selected disc's archive is {BIGFILE_SIZE}"
        )
    digest = hashlib.sha256()
    with archive.open("rb") as handle:
        while chunk := handle.read(1 << 22):
            digest.update(chunk)
    if digest.hexdigest() != BIGFILE_SHA256:
        raise ProvisionError(
            f"{archive} has SHA-256 {digest.hexdigest()}; the selected disc's archive is {BIGFILE_SHA256}"
        )


def extract(disc_argument: str | None, discdump_override: str | None) -> int:
    stems = module_entries()
    if not stems:
        print("[overlays] the title manifest declares no runtime modules; nothing to extract")
        return 0
    archive = provision_bigfile(disc_argument, discdump_override)
    verify_bigfile(archive)
    with archive.open("rb") as handle:
        entries = read_index(handle.read(BIGFILE_INDEX_BYTES))
        print(
            f"[overlays] {BIGFILE_NAME} index: {len(entries)} entries, {len(stems)} named by the title manifest"
        )
        OVERLAY_DIR.mkdir(parents=True, exist_ok=True)
        for stem, module in sorted(stems.items()):
            entry_id = module.archive_entry
            if entry_id >= len(entries):
                raise ProvisionError(
                    f"overlay {stem} names archive entry {entry_id}, but the index holds {len(entries)}"
                )
            offset, size = entries[entry_id]
            if offset != module.sector_offset or size != module.byte_size:
                raise ProvisionError(
                    f"overlay {stem}: archive entry is sector {offset} size {size}, "
                    f"manifest requires sector {module.sector_offset} size {module.byte_size}"
                )
            handle.seek(offset * SECTOR_BYTES)
            image = handle.read(size)
            if len(image) != size:
                raise ProvisionError(
                    f"overlay {stem}: archive holds {len(image)} of {size} bytes at sector {offset}"
                )
            actual_digest = fnv64(image)
            if actual_digest != module.fnv64:
                raise ProvisionError(
                    f"overlay {stem}: FNV64 {actual_digest:016X} differs from manifest {module.fnv64:016X}"
                )
            looks_like_code, counts = classify(image)
            target = OVERLAY_DIR / f"{stem}.BIN"
            staging = target.with_suffix(".BIN.partial")
            staging.write_bytes(image)
            os.replace(staging, target)
            print(
                f"[overlays] {stem}: entry {entry_id} sector {offset} size {size} -> "
                f"{target.relative_to(ROOT)} ({'code' if looks_like_code else 'DATA'}: "
                f"{counts['jr_ra']} jr-ra, {counts['jal']} jal, {counts['zero']}/{counts['words']} zero words)"
            )
    return 0


def emit_header(path: Path) -> None:
    """Generate only verified non-executable identity facts for the title runtime."""
    entries = sorted(module_entries().values(), key=lambda item: item.archive_entry)
    rows = "\n".join(
        f'    {{"{item.stem}", {item.archive_entry}u, {item.sector_offset}u, '
        f'{item.byte_size}u, 0x{item.load_address:08X}u, 0x{item.fnv64:016X}ull}},'
        for item in entries
    )
    content = (
        "// Generated from titles/ctr/overlays.json; non-executable title identity facts.\n"
        "#pragma once\n#include <array>\n#include <cstdint>\n\n"
        "namespace ctr {\n"
        "struct OverlayDescriptor {\n"
        "  const char *name;\n  uint32_t archiveEntry;\n  uint32_t sectorOffset;\n"
        "  uint32_t byteSize;\n  uint32_t loadAddress;\n  uint64_t fnv64;\n};\n"
        f"inline constexpr uint32_t kBigfileBytes = {BIGFILE_SIZE}u;\n"
        f"inline constexpr std::array<OverlayDescriptor, {len(entries)}> kOverlayDescriptors{{{{\n"
        f"{rows}\n}}}};\n}} // namespace ctr\n"
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(path.suffix + ".partial")
    staging.write_text(content, encoding="utf-8")
    os.replace(staging, path)


def selftest() -> None:
    """Prove the parser and classifier answer BOTH ways before either is trusted."""
    checks = 0

    def code_words(count: int) -> bytes:
        body = [0x27BDFFE8, 0x0C001234, 0x00000000, MIPS_JR_RA] * (count // 4)
        return struct.pack(f"<{len(body)}I", *body)

    data_words = struct.pack("<8I", *([0x11111111] * 8))

    index = bytearray(BIGFILE_INDEX_BYTES)
    struct.pack_into("<4I", index, 8, 3, 64, 4, len(data_words))
    entries = read_index(bytes(index))
    assert entries == [(3, 64), (4, len(data_words))], entries
    checks += 1

    for broken, why in (
        (b"\x00" * 16, "short index"),
        (bytes(BIGFILE_INDEX_BYTES), "empty index"),
    ):
        try:
            read_index(broken)
        except ProvisionError:
            checks += 1
        else:  # pragma: no cover - the refusal is the behaviour under test
            raise AssertionError(f"read_index accepted a {why}")

    overlapping = bytearray(BIGFILE_INDEX_BYTES)
    struct.pack_into("<4I", overlapping, 8, 3, 4096, 4, 16)
    try:
        read_index(bytes(overlapping))
    except ProvisionError:
        checks += 1
    else:  # pragma: no cover
        raise AssertionError("read_index accepted overlapping entries")

    is_code, counts = classify(code_words(64))
    assert is_code and counts["jr_ra"] == 16, counts
    checks += 1
    is_code, counts = classify(data_words)
    assert not is_code and counts["jr_ra"] == 0, counts
    checks += 1

    for stem, why in (("BIGFILE", "a non-BF stem"), ("BF12", "a short id")):
        assert STEM_PATTERN.match(stem) is None, why
        checks += 1
    assert STEM_PATTERN.match("BF0233").group(1) == "0233"
    checks += 1

    expected_modules = {
        "BF0233": {
            "archive_entry": 233,
            "sector_offset": 53566,
            "byte_size": 56844,
            "load_address": "0x800AB9F0",
            "fnv64": "0xA4E9995DC4FD53F7",
        }
    }
    assert parse_module_entries(expected_modules)["BF0233"].byte_size == 56844
    checks += 1
    assert fnv64(b"overlay") != fnv64(b"overlax")
    checks += 1
    for invalid in (
        {"BF0233": {**expected_modules["BF0233"], "archive_entry": 232}},
        {"BF0233": {**expected_modules["BF0233"], "load_address": "not-an-address"}},
        {"BF0233": {**expected_modules["BF0233"], "byte_size": -1}},
        {"BF0233": {**expected_modules["BF0233"], "fnv64": "short"}},
        {
            "BF0233": expected_modules["BF0233"],
            "BF0226": {**expected_modules["BF0233"], "archive_entry": 226},
        },
    ):
        try:
            parse_module_entries(invalid)
        except ProvisionError:
            checks += 1
        else:  # pragma: no cover
            raise AssertionError(f"module manifest accepted inconsistent policy: {invalid}")

    print(
        f"[overlays-selftest] PASS {checks}/{checks} checks: index parsing, manifest refusals, and both classifier verdicts"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "disc", nargs="?", help="CTR USA CHD; otherwise use env/.env/drop-in"
    )
    parser.add_argument("--discdump", help="shipping discdump executable override")
    parser.add_argument(
        "--selftest", action="store_true", help="run the hermetic fixture checks"
    )
    parser.add_argument(
        "--emit-header", type=Path, help="write title identity facts as a generated C++ header"
    )
    arguments = parser.parse_args()
    if arguments.selftest:
        selftest()
        return 0
    if arguments.emit_header is not None:
        if arguments.disc or arguments.discdump:
            raise ProvisionError("header emission reads only the tracked manifest, not a disc")
        emit_header(arguments.emit_header)
        return 0
    return extract(arguments.disc, arguments.discdump)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ProvisionError) as error:
        print(f"[overlays] REFUSED: {error}", file=sys.stderr)
        sys.exit(2)
