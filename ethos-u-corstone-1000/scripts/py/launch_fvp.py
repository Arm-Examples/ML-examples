#!/usr/bin/env python3
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
"""Launch a Corstone-1000 FVP for interactive manual use."""

from __future__ import annotations

import argparse
import logging
import os
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import NoReturn

from corstone1000_fvp.discovery import (
    resolve_fvp_executable,
    safe_resolve,
    workspace_config_path,
)
from corstone1000_fvp.kas import (
    KasError,
    build_kas_shell_command,
    resolve_command,
    validate_kas_inputs,
    write_top_config,
)
from corstone1000_fvp.runfvp import (
    TerminalCommand,
    build_manual_runfvp_command,
)
from corstone1000_fvp.tmux import (
    TmuxError,
    exec_tmux_session_if_needed,
    pane_terminal_commands,
    placeholder_panes,
    setup_tmux_panes,
)
from corstone1000_fvp.util import configure_logging

LOGGER = logging.getLogger(__name__)
PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUILD_DIR = PROJECT_ROOT / "cmake-build-corstone-1000-aarch64"
DEFAULT_MMC_IMAGE = DEFAULT_BUILD_DIR / "corstone-1000-mmc.img"


@dataclass(frozen=True)
class ManualLaunchConfig:
    """Resolved resources for one manual FVP launch."""

    work_dir: Path
    mmc_image: Path
    fvp_config: Path
    fvp_path: Path
    kas_path: Path


def _parse_args(argv: list[str]) -> argparse.Namespace:
    """
    Parse command line arguments.

    :param argv:    Command line arguments without program name.
    :returns:       Parsed command line arguments.
    """
    parser = argparse.ArgumentParser(
        allow_abbrev=False,
        description="Launch a Corstone-1000 FVP with the packaged MMC image.",
    )
    parser.add_argument(
        "-d",
        "--work-dir",
        default=os.environ.get("CORSTONE1000_WORK_DIR")
        or os.environ.get("CORSTONE_1000_FVP_ROOT", ""),
        help="Corstone-1000 Yocto workspace directory.",
    )
    parser.add_argument(
        "-m",
        "--mmc-image",
        default=str(DEFAULT_MMC_IMAGE),
        help="MMC image path.",
    )
    parser.add_argument(
        "-f",
        "--fvp",
        default=os.environ.get("FVP_CORSTONE1000", ""),
        help="FVP executable path.",
    )
    parser.add_argument(
        "-k",
        "--kas",
        default=os.environ.get("KAS", ""),
        help="kas executable path or command name.",
    )
    parser.add_argument(
        "--fvp-config",
        default="",
        help="runfvp .fvpconf path.",
    )
    parser.add_argument(
        "--terminal-layout",
        default="tmux-windows",
        choices=("tmux-windows", "tmux-panes"),
        help="Terminal layout.",
    )
    parser.add_argument(
        "--ssh-host-port",
        default=2222,
        type=int,
        help="Host port forwarded to target SSH port 22.",
    )
    parser.add_argument(
        "--no-user-networking",
        action="store_true",
        help="Disable user networking and SSH port forwarding.",
    )
    parser.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        help="Print the generated kas command and exit.",
    )
    return parser.parse_args(argv)


def _die(message: str) -> NoReturn:
    """
    Log an error message and terminate the process.

    :param message:     Error message to log.
    :raises SystemExit: Always raised with a failing process status.
    """
    LOGGER.error("ERROR: %s", message)
    raise SystemExit(1)


def _validate_ssh_host_port(port: int):
    """
    Validate an SSH host port value.

    :param port:    Host TCP port.
    :raises SystemExit: If the port is outside the valid TCP port range.
    """
    if port < 1 or port > 65535:
        _die(f"invalid --ssh-host-port: {port}")


def _resolve_manual_launch_config(args: argparse.Namespace) -> ManualLaunchConfig:
    """
    Resolve files and tools needed for a manual FVP launch.

    :param args:    Parsed command line arguments.
    :returns:       Resolved launch configuration.
    """
    if not args.work_dir:
        _die(
            "Corstone-1000 workspace not set. Use --work-dir, "
            "CORSTONE1000_WORK_DIR, or CORSTONE_1000_FVP_ROOT."
        )

    work_dir = safe_resolve(args.work_dir)
    mmc_image = safe_resolve(args.mmc_image)
    fvp_config = (
        safe_resolve(args.fvp_config)
        if args.fvp_config
        else workspace_config_path(work_dir)
    )
    kas_path = _resolve_kas(args.kas)
    fvp_path = resolve_fvp_executable(args.fvp or None, work_dir, fvp_config)

    try:
        validate_kas_inputs(work_dir)
    except KasError as error:
        _die(str(error))

    if not mmc_image.is_file():
        _die(f"MMC image not found: {mmc_image}")
    if not fvp_config.is_file():
        _die(
            f"FVP config not found: {fvp_config}. Build the Corstone-1000 "
            "flash firmware image or pass --fvp-config."
        )
    if not fvp_path:
        _die(
            "FVP executable not found. Use --fvp or FVP_CORSTONE1000, "
            "or build the workspace FVP provider."
        )
    if not fvp_path.is_file() or not os.access(fvp_path, os.X_OK):
        _die(f"FVP executable not found or not executable: {fvp_path}")

    return ManualLaunchConfig(work_dir, mmc_image, fvp_config, fvp_path, kas_path)


def _resolve_kas(kas_option: str) -> Path:
    """
    Resolve the kas command used by manual launch.

    :param kas_option:     CLI or environment kas value.
    :returns:              Resolved kas executable path.
    """
    try:
        return resolve_command(
            kas_option or None,
            PROJECT_ROOT / "downloads" / "env" / "bin" / "kas",
            "kas",
        )
    except KasError as error:
        _die(str(error))


def _terminal_commands(args: argparse.Namespace) -> list[TerminalCommand]:
    """
    Resolve terminal commands for the requested manual layout.

    :param args:    Parsed command line arguments.
    :returns:       FVP terminal command overrides.
    """
    if args.terminal_layout != "tmux-panes":
        return []
    panes = placeholder_panes() if args.dry_run else setup_tmux_panes()
    return pane_terminal_commands(panes)


def _launch_environment(config: ManualLaunchConfig) -> dict[str, str]:
    """
    Build the environment for the kas shell launch.

    :param config:  Resolved manual launch configuration.
    :returns:       Environment variables for the kas process.
    """
    env = os.environ.copy()
    env["KAS_WORK_DIR"] = str(config.work_dir)
    env["FVP_CORSTONE1000"] = str(config.fvp_path)
    return env


def _print_dry_run(top_config: Path, kas_command: list[str]):
    """
    Print dry-run command details.

    :param top_config:      Generated kas top configuration.
    :param kas_command:     kas command that would be executed.
    """
    print(f"Generated kas config: {top_config}")
    print(shlex.join(kas_command))


def _run_kas(kas_command: list[str], env: dict[str, str]) -> int:
    """
    Run the manual FVP launch command.

    :param kas_command:     Complete kas command argument vector.
    :param env:             Environment variables for the kas process.
    :returns:               kas process exit status.
    """
    return subprocess.run(kas_command, check=False, env=env).returncode


def main(argv: list[str] | None = None) -> int:
    """
    Launch the Corstone-1000 FVP for manual use.

    :param argv:    Optional command line arguments without program name.
    :returns:       Process status.
    """
    configure_logging()
    original_argv = sys.argv[1:] if argv is None else argv
    args = _parse_args(original_argv)
    _validate_ssh_host_port(args.ssh_host_port)

    try:
        exec_tmux_session_if_needed(
            Path(__file__).resolve(),
            original_argv,
            args.terminal_layout,
            args.dry_run,
        )
        config = _resolve_manual_launch_config(args)
        terminal_commands = _terminal_commands(args)
    except TmuxError as error:
        _die(str(error))

    runfvp_command = build_manual_runfvp_command(
        config.fvp_config,
        config.mmc_image,
        terminal_commands,
        not args.no_user_networking,
        args.ssh_host_port,
    )
    top_config = write_top_config(config.work_dir)
    kas_command = build_kas_shell_command(config.kas_path, top_config, runfvp_command)
    env = _launch_environment(config)

    try:
        if args.dry_run:
            _print_dry_run(top_config, kas_command)
            return 0
        return _run_kas(kas_command, env)
    finally:
        top_config.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
