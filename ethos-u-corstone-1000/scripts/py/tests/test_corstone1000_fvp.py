#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Tests for Corstone-1000 FVP Python launch helpers."""

from __future__ import annotations

import json
import stat
from pathlib import Path

import pytest

import launch_fvp
from corstone1000_fvp.discovery import (
    FvpLauncher,
    PACKAGE_CONFIG,
    detect_launcher,
    resolve_fvp_executable,
    workspace_config_path,
)
from corstone1000_fvp.kas import render_top_config
from corstone1000_fvp.runfvp import (
    TerminalCommand,
    build_ctest_runfvp_command,
)
from corstone1000_fvp.tmux import TmuxPanes, pane_terminal_commands


def _write_text(path: Path, content: str):
    """
    Write a text file, creating parent directories first.

    :param path:        Destination path.
    :param content:     File content.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _write_executable(path: Path):
    """
    Write a minimal executable file.

    :param path:    Destination path.
    """
    _write_text(path, "#!/bin/sh\nexit 0\n")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def _create_workspace(root: Path) -> Path:
    """
    Create a fake Corstone-1000 workspace.

    :param root:    Workspace root path.
    :returns:       Fake FVP executable path.
    """
    _write_executable(root / "meta-arm" / "scripts" / "runfvp")
    _write_text(root / "meta-arm" / "kas" / "corstone1000-fvp.yml", "header: {}\n")
    _write_text(root / "meta-arm" / "ci" / "debug.yml", "header: {}\n")
    _write_text(root / "meta-arm" / "kas" / "corstone1000-a320.yml", "header: {}\n")

    fvp_dir = root / "fvp-bin"
    fvp_executable = fvp_dir / "FVP_Corstone-1000-A320"
    _write_executable(fvp_executable)
    fvp_config = workspace_config_path(root)
    _write_text(
        fvp_config,
        json.dumps({"fvp-bindir": str(fvp_dir), "exe": fvp_executable.name}),
    )
    return fvp_executable


def test_detect_launcher_finds_workspace(tmp_path: Path):
    """Workspace discovery resolves runfvp and the generated config."""
    _create_workspace(tmp_path)

    launcher = detect_launcher(tmp_path)

    assert launcher.launcher == tmp_path / "meta-arm" / "scripts" / "runfvp"
    assert launcher.config == workspace_config_path(tmp_path)
    assert launcher.root == tmp_path


def test_detect_launcher_expands_versioned_home_package_glob(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
):
    """Home package discovery expands versioned Corstone-1000 FVP roots."""
    home = tmp_path / "home"
    older_root = home / "corstone-1000-fvp-2025.10"
    newer_root = home / "corstone-1000-fvp-2025.12"

    _write_executable(older_root / "meta-arm" / "scripts" / "runfvp")
    _write_text(older_root / PACKAGE_CONFIG, "{}")
    _write_executable(newer_root / "meta-arm" / "scripts" / "runfvp")
    _write_text(newer_root / PACKAGE_CONFIG, "{}")
    monkeypatch.setenv("HOME", str(home))
    monkeypatch.delenv("CORSTONE_1000_FVP_ROOT", raising=False)
    monkeypatch.delenv("CORSTONE1000_WORK_DIR", raising=False)

    launcher = detect_launcher()

    assert launcher.launcher == newer_root / "meta-arm" / "scripts" / "runfvp"
    assert launcher.config == newer_root / PACKAGE_CONFIG
    assert launcher.root == newer_root


def test_resolve_fvp_executable_prefers_config(tmp_path: Path):
    """FVP executable resolution uses the path declared by .fvpconf."""
    fvp_executable = _create_workspace(tmp_path)

    resolved = resolve_fvp_executable(None, tmp_path, workspace_config_path(tmp_path))

    assert resolved == fvp_executable


def test_build_ctest_runfvp_command():
    """CTest command construction preserves dynamic telnet callbacks."""
    launcher = FvpLauncher(
        Path("/workspace/meta-arm/scripts/runfvp"),
        Path("/workspace/config.fvpconf"),
        Path("/workspace"),
    )
    terminal_commands = [
        TerminalCommand("host.host_terminal_0", "record normal %port"),
        TerminalCommand("host.host_terminal_1", "record secure %port"),
    ]

    command = build_ctest_runfvp_command(
        launcher,
        Path("/build/test/corstone-1000-mmc.img"),
        terminal_commands,
    )
    command_text = " ".join(command)

    assert command[2] == "--terminals=none"
    assert "host.ethosu.num_macs=2048" in command
    assert "board.msd_mmc.p_mmc_file=/build/test/corstone-1000-mmc.img" in command
    assert "host.host_terminal_0.start_telnet=1" in command
    assert "host.host_terminal_0.start_port=0" in command
    assert "record normal %port" in command_text


def test_render_top_config_uses_workspace_meta_arm(tmp_path: Path):
    """Temporary kas top config points at the workspace meta-arm tree."""
    _create_workspace(tmp_path)

    rendered = render_top_config(tmp_path)

    assert "kas/corstone1000-fvp.yml" in rendered
    assert "ci/debug.yml" in rendered
    assert json.dumps(str(tmp_path / "meta-arm")) in rendered


def test_pane_terminal_commands_target_visible_panes():
    """tmux pane terminal commands route each console to its pane."""
    commands = pane_terminal_commands(TmuxPanes("/tmp/tmux-socket", "%1", "%2", "%3"))

    assert len(commands) == 3
    assert commands[0].terminal == "host.host_terminal_0"
    assert "tmux -S /tmp/tmux-socket" in commands[0].command
    assert "send-keys -t %1" in commands[0].command
    assert "telnet localhost %port" in commands[0].command


def test_manual_dry_run_builds_tmux_pane_command(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
):
    """Manual dry-run emits kas/runfvp command with tmux pane overrides."""
    fvp_executable = _create_workspace(tmp_path)
    kas_path = tmp_path / "downloads" / "env" / "bin" / "kas"
    mmc_image = (
        tmp_path / "cmake-build-corstone-1000-aarch64" / "corstone-1000-mmc.img"
    )
    _write_executable(kas_path)
    _write_text(mmc_image, "fake image\n")

    status = launch_fvp.main(
        [
            "--work-dir",
            str(tmp_path),
            "--mmc-image",
            str(mmc_image),
            "--fvp",
            str(fvp_executable),
            "--kas",
            str(kas_path),
            "--terminal-layout",
            "tmux-panes",
            "--dry-run",
        ]
    )

    output = capsys.readouterr().out
    assert status == 0
    assert "kas shell" in output
    assert "../meta-arm/scripts/runfvp" in output
    assert "host.ethosu.num_macs=2048" in output
    assert "board.hostbridge.userNetworking=1" in output
    assert "board.hostbridge.userNetPorts=2222=22" in output
    assert "host.host_terminal_0.terminal_command" in output
    assert "telnet localhost %port" in output
