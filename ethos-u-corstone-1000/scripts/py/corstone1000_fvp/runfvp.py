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
"""runfvp command construction for Corstone-1000 launch flows."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from corstone1000_fvp.discovery import FvpLauncher

ETHOSU_MACS_PARAMETER = "host.ethosu.num_macs"
ETHOSU_MACS_VALUE = "2048"
MMC_PARAMETER = "board.msd_mmc.p_mmc_file"


@dataclass(frozen=True)
class TerminalCommand:
    """FVP terminal callback command for one terminal endpoint."""

    terminal: str
    command: str


def model_parameter(key: str, value: str | Path) -> list[str]:
    """
    Return one ``-C`` model parameter argument pair.

    :param key:     Model parameter name.
    :param value:   Model parameter value.
    :returns:       ``runfvp`` command arguments for the parameter.
    """
    return ["-C", f"{key}={value}"]


def common_model_parameters(mmc_image: Path) -> list[str]:
    """
    Return model parameters shared by manual and CTest launches.

    :param mmc_image:   MMC image to attach to the FVP.
    :returns:           Common ``runfvp`` model parameter arguments.
    """
    command: list[str] = []
    command.extend(model_parameter(ETHOSU_MACS_PARAMETER, ETHOSU_MACS_VALUE))
    command.extend(model_parameter(MMC_PARAMETER, mmc_image))
    return command


def terminal_telnet_parameters(terminal_commands: list[TerminalCommand]) -> list[str]:
    """
    Return dynamic telnet configuration for FVP terminals.

    :param terminal_commands:     Terminal command callbacks to configure.
    :returns:                     ``runfvp`` model parameter arguments.
    """
    command: list[str] = []
    for terminal_command in terminal_commands:
        command.extend(model_parameter(f"{terminal_command.terminal}.start_telnet", "1"))
        command.extend(model_parameter(f"{terminal_command.terminal}.start_port", "0"))
        command.extend(
            model_parameter(
                f"{terminal_command.terminal}.terminal_command",
                terminal_command.command,
            )
        )
    return command


def build_ctest_runfvp_command(
    launcher: FvpLauncher,
    mmc_image: Path,
    terminal_commands: list[TerminalCommand],
) -> list[str]:
    """
    Build the non-interactive CTest ``runfvp`` command line.

    :param launcher:              Resolved FVP launcher paths.
    :param mmc_image:             Per-test MMC image path.
    :param terminal_commands:     Dynamic terminal callbacks.
    :returns:                     Complete command argument vector.
    """
    command = [
        str(launcher.launcher),
        str(launcher.config),
        "--terminals=none",
        "--",
    ]
    command.extend(common_model_parameters(mmc_image))
    command.extend(terminal_telnet_parameters(terminal_commands))
    return command


def user_networking_parameters(ssh_host_port: int) -> list[str]:
    """
    Return user-networking model parameters for manual FVP launches.

    :param ssh_host_port:     Host port to forward to target SSH port 22.
    :returns:                 ``runfvp`` model parameter arguments.
    """
    command: list[str] = []
    command.extend(model_parameter("board.hostbridge.userNetworking", "1"))
    command.extend(model_parameter("board.hostbridge.userNetPorts", f"{ssh_host_port}=22"))
    return command


def terminal_command_parameters(
    terminal_commands: list[TerminalCommand],
) -> list[str]:
    """
    Return terminal command overrides without changing telnet start ports.

    :param terminal_commands:     Terminal command callbacks to configure.
    :returns:                     ``runfvp`` model parameter arguments.
    """
    command: list[str] = []
    for terminal_command in terminal_commands:
        command.extend(
            model_parameter(
                f"{terminal_command.terminal}.terminal_command",
                terminal_command.command,
            )
        )
    return command


def build_manual_runfvp_command(
    fvp_config: Path,
    mmc_image: Path,
    terminal_commands: list[TerminalCommand],
    user_networking: bool,
    ssh_host_port: int,
) -> list[str]:
    """
    Build the interactive manual ``runfvp`` command line.

    :param fvp_config:            runfvp ``.fvpconf`` path.
    :param mmc_image:             MMC image to attach to the FVP.
    :param terminal_commands:     Optional terminal command overrides.
    :param user_networking:       Enable user networking and SSH forwarding.
    :param ssh_host_port:         Host SSH port to forward when enabled.
    :returns:                     Complete command argument vector.
    """
    command = [
        "../meta-arm/scripts/runfvp",
        str(fvp_config),
        "--terminals=tmux",
        "--",
    ]
    command.extend(common_model_parameters(mmc_image))
    if user_networking:
        command.extend(user_networking_parameters(ssh_host_port))
    command.extend(terminal_command_parameters(terminal_commands))
    return command
