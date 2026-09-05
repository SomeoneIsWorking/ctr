#!/usr/bin/env python3
"""Hermetic tests for the framework build-provenance check."""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import psxport_sync


class ResolvedBuildContractTest(unittest.TestCase):
    def setUp(self) -> None:
        (Path(psxport_sync.REPO) / "scratch").mkdir(exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(dir=Path(psxport_sync.REPO) / "scratch")
        self.addCleanup(self.scratch.cleanup)
        self.head = mock.patch.object(psxport_sync, "head_of", return_value="1" * 40).start()
        self.dirty = mock.patch.object(psxport_sync, "dirty", return_value=False).start()
        self.addCleanup(mock.patch.stopall)

    def write_resolved(self, commit: str) -> Path:
        resolved = Path(self.scratch.name) / "psxport_resolved.txt"
        resolved.write_text(
            f"dir = /portable/framework\ncommit = {commit}\n", encoding="utf-8"
        )
        return resolved

    def test_check_reads_the_selected_build_tree_record(self) -> None:
        commit = "1" * 40
        selected = self.write_resolved(commit)
        with mock.patch.object(
            psxport_sync, "read_pin", return_value=("https://example.invalid/psxport.git", commit)
        ):
            result = psxport_sync.do_check(SimpleNamespace(resolved=str(selected)))

        self.assertEqual(result, 0)

    def test_check_refuses_missing_build_record(self) -> None:
        with mock.patch.object(
            psxport_sync, "read_pin", return_value=("https://example.invalid/psxport.git", "1" * 40)
        ):
            result = psxport_sync.do_check(SimpleNamespace(resolved=str(Path(self.scratch.name) / "absent")))
        self.assertEqual(result, 2)

    def test_check_rejects_a_selected_build_record_from_another_commit(self) -> None:
        selected = self.write_resolved("2" * 40)
        self.head.return_value = "2" * 40
        output = io.StringIO()
        with (
            mock.patch.object(
                psxport_sync,
                "read_pin",
                return_value=("https://example.invalid/psxport.git", "1" * 40),
            ),
            contextlib.redirect_stdout(output),
        ):
            result = psxport_sync.do_check(SimpleNamespace(resolved=str(selected)))

        self.assertEqual(result, 1)
        self.assertIn("built against 22222222", output.getvalue())
        self.assertIn("records 11111111", output.getvalue())

    def test_check_rejects_uncommitted_framework_changes(self) -> None:
        selected = self.write_resolved("1" * 40)
        self.dirty.return_value = True
        with mock.patch.object(
            psxport_sync, "read_pin", return_value=("https://example.invalid/psxport.git", "1" * 40)
        ):
            result = psxport_sync.do_check(SimpleNamespace(resolved=str(selected)))
        self.assertEqual(result, 1)

    def test_check_rejects_checkout_moved_since_configure(self) -> None:
        selected = self.write_resolved("1" * 40)
        self.head.return_value = "2" * 40
        with mock.patch.object(
            psxport_sync, "read_pin", return_value=("https://example.invalid/psxport.git", "1" * 40)
        ):
            result = psxport_sync.do_check(SimpleNamespace(resolved=str(selected)))
        self.assertEqual(result, 1)


if __name__ == "__main__":
    unittest.main()
