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
"""tmux integration helpers for interactive Corstone-1000 FVP launches."""

from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from corstone1000_fvp.runfvp import TerminalCommand

TERMINAL_PANES = (
    ("host.host_terminal_0", "normal"),
    ("host.host_terminal_1", "secure"),
    ("se.secenc_terminal", "enclave"),
)


class TmuxError(RuntimeError):
    """Failure while preparing a tmux session or pane layout."""


@dataclass(frozen=True)
class TmuxPanes:
    """Pre-created tmux panes for the Corstone-1000 FVP consoles."""

    socket: str
    normal: str
    secure: str
    enclave: str


def tmux_path() -> str:
    """
    Resolve the tmux executable path.

    :returns:           tmux executable path.
    :raises TmuxError:  If tmux is not available on ``PATH``.
    """
    command_path = shutil.which("tmux")
    if command_path:
        return command_path
    raise TmuxError("tmux not found on PATH.")


def exec_tmux_session_if_needed(
    script_path: Path,
    argv: list[str],
    terminal_layout: str,
    dry_run: bool,
):
    """
    Replace the current process with tmux when pane layout needs a session.

    :param script_path:         Launcher script path.
    :param argv:                Original launcher arguments.
    :param terminal_layout:     Requested terminal layout.
    :param dry_run:             Whether command generation only was requested.
    :returns:                   ``None`` when no tmux bootstrap is needed.
    :raises TmuxError:          If a new tmux session cannot be started.
    """
    if terminal_layout != "tmux-panes" or dry_run or os.environ.get("TMUX"):
        return None
    if not os.isatty(0) or not os.isatty(1):
        raise TmuxError(
            "--terminal-layout tmux-panes outside tmux requires an interactive "
            "terminal."
        )

    tmux = tmux_path()
    session_name = f"corstone-1000-fvp-{os.getpid()}"
    helper_command = shlex.join([sys.executable, str(script_path), *argv])
    tmux_command = (
        f"{helper_command}; helper_status=$?; "
        "printf '\\nFVP helper exited with status %s. "
        "Press Ctrl-D to close this tmux session.\\n' \"${helper_status}\"; "
        "exec \"${SHELL:-/bin/sh}\""
    )
    os.execvp(
        tmux,
        [
            tmux,
            "new-session",
            "-s",
            session_name,
            "-c",
            str(Path.cwd()),
            tmux_command,
        ],
    )
    raise TmuxError("failed to replace process with tmux")


def _run_tmux(
    socket: str,
    arguments: list[str],
    capture: bool,
) -> subprocess.CompletedProcess[str]:
    """
    Run one tmux command.

    :param socket:      tmux socket path.
    :param arguments:   tmux command arguments after ``tmux -S <socket>``.
    :param capture:     Capture and return stdout when true.
    :returns:           Completed tmux process.
    """
    return subprocess.run(
        [tmux_path(), "-S", socket, *arguments],
        check=False,
        capture_output=capture,
        text=True,
    )


def _require_tmux_success(process: subprocess.CompletedProcess[str], action: str):
    """
    Raise ``TmuxError`` if a tmux command failed.

    :param process:     Completed tmux process.
    :param action:      Human-readable action description.
    :raises TmuxError:  If the tmux process failed.
    """
    if process.returncode != 0:
        detail = process.stderr.strip() if process.stderr else "unknown error"
        raise TmuxError(f"tmux failed to {action}: {detail}")


def tmux_socket_from_environment() -> str:
    """
    Return the active tmux socket from ``TMUX``.

    :returns:           tmux socket path.
    :raises TmuxError:  If the current process is not inside tmux.
    """
    tmux_env = os.environ.get("TMUX", "")
    if not tmux_env:
        raise TmuxError("--terminal-layout tmux-panes must be run from inside tmux.")

    socket = tmux_env.split(",", maxsplit=1)[0]
    if not Path(socket).is_socket():
        raise TmuxError(f"tmux socket not found: {socket}")
    return socket


def setup_tmux_panes() -> TmuxPanes:
    """
    Create a tmux window with panes for the Corstone-1000 FVP consoles.

    :returns:           Created tmux socket and pane identifiers.
    :raises TmuxError:  If pane creation fails.
    """
    socket = tmux_socket_from_environment()
    new_window = _run_tmux(
        socket,
        [
            "new-window",
            "-d",
            "-P",
            "-F",
            "#{window_id} #{pane_id}",
            "-n",
            "Corstone-1000 FVP",
        ],
        True,
    )
    _require_tmux_success(new_window, "create FVP window")

    tmux_info = new_window.stdout.strip().split()
    if len(tmux_info) != 2:
        raise TmuxError(f"unexpected tmux window response: {new_window.stdout!r}")

    window_id = tmux_info[0]
    first_pane = tmux_info[1]
    second_pane_process = _run_tmux(
        socket,
        ["split-window", "-d", "-h", "-t", first_pane, "-P", "-F", "#{pane_id}"],
        True,
    )
    _require_tmux_success(second_pane_process, "create secure-world pane")
    third_pane_process = _run_tmux(
        socket,
        [
            "split-window",
            "-d",
            "-v",
            "-t",
            second_pane_process.stdout.strip(),
            "-P",
            "-F",
            "#{pane_id}",
        ],
        True,
    )
    _require_tmux_success(third_pane_process, "create secure-enclave pane")

    panes = TmuxPanes(
        socket,
        first_pane,
        second_pane_process.stdout.strip(),
        third_pane_process.stdout.strip(),
    )
    configure_pane_titles(window_id, panes)
    return panes


def configure_pane_titles(window_id: str, panes: TmuxPanes):
    """
    Set tmux pane titles and layout for an FVP console window.

    :param window_id:   tmux window identifier.
    :param panes:       Created FVP console panes.
    :raises TmuxError:  If tmux cannot configure the window.
    """
    commands = [
        (["select-pane", "-t", panes.normal, "-T", "Normal World Console"], "title"),
        (["select-pane", "-t", panes.secure, "-T", "Secure World Console"], "title"),
        (
            ["select-pane", "-t", panes.enclave, "-T", "Secure Enclave Console"],
            "title",
        ),
        (
            ["set-window-option", "-t", window_id, "pane-border-status", "top"],
            "set pane border status",
        ),
        (
            [
                "set-window-option",
                "-t",
                window_id,
                "pane-border-format",
                "#{pane_title}",
            ],
            "set pane border format",
        ),
        (["select-layout", "-t", window_id, "tiled"], "select tiled layout"),
        (["select-window", "-t", window_id], "select FVP window"),
    ]

    for command, action in commands:
        process = _run_tmux(panes.socket, command, False)
        _require_tmux_success(process, action)


def placeholder_panes() -> TmuxPanes:
    """
    Return placeholder pane identifiers for dry-run output.

    :returns:   Placeholder pane configuration.
    """
    return TmuxPanes(
        "<tmux-socket>",
        "<normal-pane>",
        "<secure-pane>",
        "<enclave-pane>",
    )


def pane_terminal_commands(panes: TmuxPanes) -> list[TerminalCommand]:
    """
    Build FVP terminal command overrides for pre-created tmux panes.

    :param panes:   tmux panes to receive terminal connections.
    :returns:       FVP terminal command callbacks.
    """
    pane_by_name = {
        "normal": panes.normal,
        "secure": panes.secure,
        "enclave": panes.enclave,
    }
    tmux_prefix = f"tmux -S {shlex.quote(panes.socket)}"
    terminal_commands: list[TerminalCommand] = []
    for terminal, pane_name in TERMINAL_PANES:
        pane = pane_by_name[pane_name]
        command = (
            f"{tmux_prefix} send-keys -t {shlex.quote(pane)} "
            "'telnet localhost %port' C-m"
        )
        terminal_commands.append(TerminalCommand(terminal, command))
    return terminal_commands
