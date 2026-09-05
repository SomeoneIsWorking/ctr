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

MIPS_JR_RA = 0x03E00008


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


def parse_module_entries(modules: object) -> dict[str, int]:
    """Validate title module policy and map each module to its BIGFILE entry."""
    if not isinstance(modules, dict):
        raise ProvisionError("overlay manifest root must be an object")
    stems: dict[str, int] = {}
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
        if not isinstance(entry, int) or entry != int(matched.group(1), 10):
            raise ProvisionError(f"overlay {stem} has inconsistent archive_entry {entry!r}")
        load_address = details.get("load_address")
        if not isinstance(load_address, str) or ADDRESS_PATTERN.fullmatch(load_address) is None:
            raise ProvisionError(f"overlay {stem} has invalid load_address {load_address!r}")
        stems[stem] = entry
    return stems


def module_entries(path: Path = OVERLAY_MANIFEST) -> dict[str, int]:
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
        for stem, entry_id in sorted(stems.items()):
            if entry_id >= len(entries):
                raise ProvisionError(
                    f"overlay {stem} names archive entry {entry_id}, but the index holds {len(entries)}"
                )
            offset, size = entries[entry_id]
            handle.seek(offset * SECTOR_BYTES)
            image = handle.read(size)
            if len(image) != size:
                raise ProvisionError(
                    f"overlay {stem}: archive holds {len(image)} of {size} bytes at sector {offset}"
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

    expected_modules = {"BF0233": {"archive_entry": 233, "load_address": "0x800AB9F0"}}
    assert parse_module_entries(expected_modules) == {"BF0233": 233}
    checks += 1
    for invalid in (
        {"BF0233": {"archive_entry": 232, "load_address": "0x800AB9F0"}},
        {"BF0233": {"archive_entry": 233, "load_address": "not-an-address"}},
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
    arguments = parser.parse_args()
    if arguments.selftest:
        selftest()
        return 0
    return extract(arguments.disc, arguments.discdump)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ProvisionError) as error:
        print(f"[overlays] REFUSED: {error}", file=sys.stderr)
        sys.exit(2)
