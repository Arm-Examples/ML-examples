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
"""FVP launcher detection and process management."""

from __future__ import annotations

import os
import re
import shlex
import signal
import subprocess
import sys
from pathlib import Path

from corstone1000_fvp.discovery import (
    FvpDiscoveryError,
    FvpLauncher,
    detect_launcher as detect_shared_launcher,
)
from corstone1000_fvp.runfvp import TerminalCommand, build_ctest_runfvp_command
from fvp_ctest.config import RunnerConfig
from fvp_ctest.status import die, skip_prerequisite


def record_port(output_file: Path, port: str):
    """
    Record one dynamic FVP terminal port.

    :param output_file:     File to write.
    :param port:            Port reported by the FVP terminal callback.
    """
    # FVP substitutes %port into a shell command. Accept only decimal TCP ports
    # before writing the callback result into a file consumed by the runner.
    if not re.fullmatch(r"[0-9]+", port):
        die(f"invalid terminal port: {port}")
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(f"{port}\n", encoding="utf-8")


def detect_launcher() -> FvpLauncher:
    """
    Detect a packaged Corstone-1000 FVP root or Yocto workspace.

    :returns:   Resolved FVP launcher paths.
    """
    try:
        return detect_shared_launcher()
    except FvpDiscoveryError as error:
        skip_prerequisite(str(error))


def _record_port_command(config: RunnerConfig, port_file: Path) -> str:
    """
    Build the FVP terminal callback command for dynamic port recording.

    :param config:      Runner configuration.
    :param port_file:   Port file to write from the callback.
    :returns:           Shell command passed to FVP terminal_command.
    """
    # start_port=0 lets the FVP choose free ports; terminal_command is the
    # callback path that reports those chosen ports back to this runner.
    return " ".join(
        [
            shlex.quote(sys.executable),
            shlex.quote(str(config.runner_script)),
            "record-port",
            shlex.quote(str(port_file)),
            "%port",
        ]
    )


def _terminal_commands(config: RunnerConfig) -> list[TerminalCommand]:
    """
    Build dynamic FVP terminal callbacks for the CTest runner.

    :param config:  Runner configuration.
    :returns:       Terminal callbacks for each FVP console.
    """
    return [
        TerminalCommand(
            "host.host_terminal_0",
            _record_port_command(config, config.log_dir / "host-terminal-0.port"),
        ),
        TerminalCommand(
            "host.host_terminal_1",
            _record_port_command(config, config.log_dir / "host-terminal-1.port"),
        ),
        TerminalCommand(
            "se.secenc_terminal",
            _record_port_command(config, config.log_dir / "secure-enclave.port"),
        ),
    ]


def _launch_environment(launcher: FvpLauncher) -> dict[str, str]:
    """
    Build the environment for a CTest FVP launch.

    :param launcher:    Resolved FVP launcher paths.
    :returns:           Environment variables for the FVP process.
    """
    env = os.environ.copy()
    if launcher.library_path:
        env["LD_LIBRARY_PATH"] = (
            f"{launcher.library_path}:{env['LD_LIBRARY_PATH']}"
            if env.get("LD_LIBRARY_PATH")
            else str(launcher.library_path)
        )
    return env


def start_fvp(config: RunnerConfig, launcher: FvpLauncher):
    """
    Start the FVP with the per-test MMC image attached.

    :param config:      Runner configuration.
    :param launcher:    Resolved FVP launcher paths.
    :returns:           FVP process and opened FVP log handle.
    """
    command = build_ctest_runfvp_command(
        launcher,
        config.app_image,
        _terminal_commands(config),
    )

    fvp_log_handle = (config.log_dir / "fvp.log").open("wb")
    try:
        # The runner owns this process until stop_fvp() tears it down after
        # console handling completes.
        fvp_process = subprocess.Popen(  # pylint: disable=consider-using-with
            command,
            env=_launch_environment(launcher),
            stdin=subprocess.DEVNULL,
            stdout=fvp_log_handle,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    except OSError:
        fvp_log_handle.close()
        raise
    return fvp_process, fvp_log_handle


def stop_fvp(fvp_process: subprocess.Popen | None, fvp_log_handle):
    """
    Stop the FVP process group and close the FVP log handle.

    :param fvp_process:     Running FVP process.
    :param fvp_log_handle:  Open FVP log handle.
    """
    if fvp_process:
        if fvp_process.poll() is None:
            try:
                os.killpg(fvp_process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                fvp_process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(fvp_process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                fvp_process.wait(timeout=5)
        else:
            fvp_process.wait()

    if fvp_log_handle:
        fvp_log_handle.close()
