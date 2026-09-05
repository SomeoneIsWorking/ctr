#!/usr/bin/env python3
"""Hermetic contract tests for the CTR product launcher."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import run


class FakeHost:
    def __init__(self, *, system: str = "Linux", distribution: str = "fedora") -> None:
        self._system = system
        self._distribution = distribution
        self.missing: set[str] = set()
        self.failed_modules: set[str] = set()
        self.commands: list[list[str]] = []

    def which(self, name: str) -> str | None:
        return None if name in self.missing else f"/mock/{name}"

    def run(self, args, **_kwargs) -> subprocess.CompletedProcess[str]:
        command = [str(value) for value in args]
        self.commands.append(command)
        failed = (
            command[:2] == ["pkg-config", "--exists"]
            and command[2] in self.failed_modules
        )
        return subprocess.CompletedProcess(command, 1 if failed else 0, "", "")

    def system(self) -> str:
        return self._system

    def linux_distribution(self) -> str:
        return self._distribution


class LauncherContractTest(unittest.TestCase):
    def test_zero_arguments_select_shipping_product(self) -> None:
        arguments = run.parse_args([])
        self.assertIsNone(arguments.disc)
        self.assertFalse(arguments.prepare_only)
        self.assertFalse(arguments.headless)

    def test_explicit_compiler_names_are_capability_probed_not_identity_filtered(
        self,
    ) -> None:
        host = FakeHost()
        cc, cxx = run.preflight(host, {"CC": "vendor-c", "CXX": "vendor-cxx"})
        self.assertEqual(cc, "/mock/vendor-c")
        self.assertEqual(cxx, "/mock/vendor-cxx")
        self.assertIn(
            ["/mock/vendor-c", "-std=c11", "-x", "c", "-fsyntax-only", "-"],
            host.commands,
        )
        self.assertIn(
            ["/mock/vendor-cxx", "-std=c++20", "-x", "c++", "-fsyntax-only", "-"],
            host.commands,
        )

    def test_missing_fedora_dependency_names_exact_user_command(self) -> None:
        host = FakeHost()
        host.failed_modules.add("sdl3-image")
        with self.assertRaisesRegex(run.Refusal, r"sudo dnf install SDL3_image-devel"):
            run.preflight(host, {})

    def test_missing_debian_tool_names_exact_user_command(self) -> None:
        host = FakeHost(distribution="ubuntu debian")
        host.missing.add("glslc")
        with self.assertRaisesRegex(run.Refusal, r"sudo apt install glslc"):
            run.preflight(host, {})

    def test_prepare_only_does_not_launch(self) -> None:
        calls: list[object] = []
        framework = Path("/mock/psxport")

        def fail_launch(*_args, **_kwargs) -> None:
            self.fail("prepare-only must not launch ctr_port")

        run.execute(
            None,
            prepare_only=True,
            preflight_step=lambda: ("/mock/cc", "/mock/cxx"),
            sync_step=lambda: framework,
            prepare_step=lambda *args: calls.append(args) or Path("/mock/ctr_port"),
            launch_step=fail_launch,
        )
        self.assertEqual(calls, [(None, framework, "/mock/cc", "/mock/cxx")])

    def test_configure_preserves_locked_python_and_shipping_build_mode(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(run, "command") as command:
            run.configure(run.BUILD_ROOT, run.ROOT / "external/psxport", "/mock/cc", "/mock/cxx")
        arguments = command.call_args.args[0]
        self.assertIn(f"-DPython3_EXECUTABLE={sys.executable}", arguments)
        self.assertIn("-DBUILD_TESTING=OFF", arguments)
        self.assertIn("-DPSXPORT_BUILD_TESTS=OFF", arguments)
        self.assertIn("Ninja", arguments)

    def test_configure_refuses_invalid_runtime_dependency_before_building(self) -> None:
        with (
            mock.patch.dict(os.environ, {"PSXPORT_LIGHTREC_DIR": "/missing/lightrec"}, clear=True),
            mock.patch.object(run, "command") as command,
            self.assertRaisesRegex(run.Refusal, "PSXPORT_LIGHTREC_DIR is incomplete"),
        ):
            run.configure(run.BUILD_ROOT, run.ROOT / "external/psxport", "/mock/cc", "/mock/cxx")
        command.assert_not_called()

    def test_default_path_prepares_then_launches_product(self) -> None:
        calls: list[object] = []
        framework = Path("/mock/psxport")
        run.execute(
            "disc.chd",
            preflight_step=lambda: ("/mock/cc", "/mock/cxx"),
            sync_step=lambda: framework,
            prepare_step=lambda *args: calls.append(("prepare", args)) or Path("/mock/ctr_port"),
            launch_step=lambda *args, **kwargs: calls.append(("launch", args, kwargs)),
        )
        self.assertEqual(
            calls,
            [
                ("prepare", ("disc.chd", framework, "/mock/cc", "/mock/cxx")),
                ("launch", (framework, Path("/mock/ctr_port")), {"headless": False}),
            ],
        )

    def test_player_and_agent_exec_environments_use_shared_policy(self) -> None:
        framework = run.ROOT / "external/psxport"
        poisoned = {
            "PSXPORT_VK_WINDOW": "1",
            "PSXPORT_VK_HEADLESS": "1",
            "PSXPORT_NOAUDIO": "1",
            "PSXPORT_NOPACE": "1",
            "PSXPORT_ASSET_DIR": "/poisoned/framework",
            "KEEP": "yes",
        }
        with (
            mock.patch.dict(os.environ, poisoned, clear=True),
            mock.patch.object(run.os, "execve") as execute,
        ):
            run.launch(framework, Path("/mock/ctr_port"), headless=False)
            player = execute.call_args.args[2]
            run.launch(framework, Path("/mock/ctr_port"), headless=True)
            agent = execute.call_args.args[2]

        self.assertEqual(player["PSXPORT_VK_WINDOW"], "1")
        self.assertEqual(player["KEEP"], "yes")
        self.assertEqual(player["PSXPORT_ASSET_DIR"], str(framework))
        self.assertEqual(agent["PSXPORT_ASSET_DIR"], str(framework))
        for key in ("PSXPORT_VK_HEADLESS", "PSXPORT_NOAUDIO", "PSXPORT_NOPACE"):
            self.assertNotIn(key, player)
            self.assertEqual(agent[key], "1")
        self.assertNotIn("PSXPORT_VK_WINDOW", agent)

    def test_launch_supplies_framework_asset_root_when_environment_is_missing(self) -> None:
        framework = run.ROOT / "external/psxport"
        with (
            mock.patch.dict(os.environ, {"KEEP": "yes"}, clear=True),
            mock.patch.object(run.os, "execve") as execute,
        ):
            run.launch(framework, Path("/mock/ctr_port"), headless=False)

        environment = execute.call_args.args[2]
        self.assertEqual(environment["PSXPORT_ASSET_DIR"], str(framework))
        self.assertEqual(environment["KEEP"], "yes")

    def test_shell_shim_enters_only_frozen_uv_bootstrap(self) -> None:
        shim = (run.ROOT / "run.sh").read_text(encoding="utf-8")
        self.assertEqual(
            shim,
            '#!/bin/sh\ncd "$(dirname "$0")" || exit 1\n'
            'exec uv run --frozen python bootstrap.py "$@"\n',
        )


if __name__ == "__main__":
    unittest.main()
