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
"""Discovery helpers for Corstone-1000 FVP launch resources."""

from __future__ import annotations

import glob
import json
import os
import platform
from dataclasses import dataclass
from json import JSONDecodeError
from pathlib import Path

DEFAULT_FVP_EXE = "FVP_Corstone-1000-A320"
DEFAULT_FVP_PROVIDER = "fvp-corstone1000-native"
WORKSPACE_CONFIG = (
    "build/tmp/deploy/images/corstone1000-fvp/"
    "corstone1000-flash-firmware-image-corstone1000-fvp.fvpconf"
)
PACKAGE_CONFIG = "corstone1000-flash-firmware-image-corstone1000-fvp.fvpconf"


class FvpDiscoveryError(RuntimeError):
    """Failure to resolve required Corstone-1000 FVP launch resources."""


@dataclass(frozen=True)
class FvpLauncher:
    """Resolved Corstone-1000 FVP launcher paths."""

    launcher: Path
    config: Path
    root: Path
    library_path: Path | None = None


def safe_resolve(path: str | Path) -> Path:
    """
    Resolve a path without requiring the final path to exist.

    :param path:    Path to normalize.
    :returns:       Absolute normalized path.
    """
    return Path(path).expanduser().resolve(strict=False)


def workspace_config_path(work_dir: Path) -> Path:
    """
    Return the default workspace-generated runfvp configuration path.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :returns:           Default workspace ``.fvpconf`` path.
    """
    return work_dir / WORKSPACE_CONFIG


def package_python_library_path(root: Path) -> Path:
    """
    Return the packaged FVP Python library directory for a root.

    :param root:    Packaged FVP root path.
    :returns:       Expected packaged Python library path.
    """
    return (
        root
        / "fvp"
        / "usr"
        / "lib"
        / "fvp"
        / "fvp-corstone1000"
        / "python"
        / "lib"
    )


def expand_candidate_roots(raw_candidate: str | Path) -> list[Path]:
    """
    Return concrete root paths for an exact candidate or glob pattern.

    :param raw_candidate:   Candidate root path or glob pattern.
    :returns:               Resolved root paths in preference order.
    """
    expanded_candidate = str(Path(raw_candidate).expanduser())
    if not glob.has_magic(expanded_candidate):
        return [safe_resolve(raw_candidate)]

    matches = sorted(
        (safe_resolve(match) for match in glob.glob(expanded_candidate)),
        reverse=True,
    )
    return [match for match in matches if match.is_dir()]


def detect_launcher(explicit_root: str | Path | None = None) -> FvpLauncher:
    """
    Detect a packaged Corstone-1000 FVP root or Yocto workspace.

    :param explicit_root:       Optional root path to check before defaults.
    :returns:                   Resolved FVP launcher paths.
    :raises FvpDiscoveryError:  If no usable launcher root is found.
    """
    env_root = os.environ.get("CORSTONE_1000_FVP_ROOT") or os.environ.get(
        "CORSTONE1000_WORK_DIR", ""
    )
    candidates = [
        explicit_root,
        env_root,
        Path.home() / "corstone-1000-fvp-*",
        Path("/opt/corstone-1000"),
    ]

    index = 0
    while index < len(candidates):
        raw_candidate = candidates[index]
        index += 1
        if not raw_candidate:
            continue
        candidate_index = 0
        candidate_roots = expand_candidate_roots(raw_candidate)
        while candidate_index < len(candidate_roots):
            candidate = candidate_roots[candidate_index]
            candidate_index += 1
            launcher = candidate / "meta-arm" / "scripts" / "runfvp"
            package_config = candidate / PACKAGE_CONFIG
            package_library_path = package_python_library_path(candidate)
            workspace_config = workspace_config_path(candidate)

            if (
                launcher.is_file()
                and os.access(launcher, os.X_OK)
                and package_config.is_file()
            ):
                library_path = (
                    package_library_path if package_library_path.is_dir() else None
                )
                return FvpLauncher(launcher, package_config, candidate, library_path)
            if (
                launcher.is_file()
                and os.access(launcher, os.X_OK)
                and workspace_config.is_file()
            ):
                return FvpLauncher(launcher, workspace_config, candidate)

    raise FvpDiscoveryError(
        "set CORSTONE_1000_FVP_ROOT, or CORSTONE1000_WORK_DIR, to a "
        "packaged Corstone-1000 FVP root or a Yocto workspace containing "
        "meta-arm/scripts/runfvp and "
        "build/tmp/deploy/images/corstone1000-fvp/"
        "corstone1000-flash-firmware-image-corstone1000-fvp.fvpconf"
    )


def fvp_path_from_config(config_path: Path) -> Path | None:
    """
    Resolve the FVP executable path declared by a runfvp configuration.

    :param config_path:     runfvp ``.fvpconf`` path.
    :returns:               FVP executable path, or ``None`` when absent.
    """
    if not config_path.is_file():
        return None

    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, JSONDecodeError):
        return None

    fvp_bindir = config.get("fvp-bindir") or ""
    fvp_exe = config.get("exe") or ""
    if fvp_bindir and fvp_exe:
        return Path(fvp_bindir) / fvp_exe
    return None


def default_fvp_candidates(work_dir: Path) -> list[Path]:
    """
    Return default FVP executable candidates inside a Yocto workspace.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :returns:           Candidate executable paths in preference order.
    """
    host_arch = platform.machine()
    return [
        work_dir
        / "build"
        / "tmp"
        / "sysroots-components"
        / host_arch
        / DEFAULT_FVP_PROVIDER
        / "usr"
        / "bin"
        / DEFAULT_FVP_EXE,
        work_dir
        / "build"
        / "tmp"
        / "sysroots-components"
        / "x86_64"
        / DEFAULT_FVP_PROVIDER
        / "usr"
        / "bin"
        / DEFAULT_FVP_EXE,
        work_dir
        / "build"
        / "tmp"
        / "sysroots-components"
        / "aarch64"
        / DEFAULT_FVP_PROVIDER
        / "usr"
        / "bin"
        / DEFAULT_FVP_EXE,
        work_dir / DEFAULT_FVP_EXE,
    ]


def first_executable_path(candidates: list[Path]) -> Path | None:
    """
    Return the first executable path from a candidate list.

    :param candidates:  Candidate paths.
    :returns:           First executable path, or ``None``.
    """
    index = 0
    while index < len(candidates):
        candidate = candidates[index]
        index += 1
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def resolve_fvp_executable(
    explicit_path: str | None,
    work_dir: Path,
    config_path: Path,
) -> Path | None:
    """
    Resolve the Corstone-1000 FVP executable path.

    :param explicit_path:   Explicit executable path from CLI or environment.
    :param work_dir:        Corstone-1000 Yocto workspace path.
    :param config_path:     runfvp ``.fvpconf`` path.
    :returns:               Resolved executable path, or ``None``.
    """
    if explicit_path:
        return safe_resolve(explicit_path)

    config_fvp = fvp_path_from_config(config_path)
    if config_fvp and config_fvp.is_file() and os.access(config_fvp, os.X_OK):
        return safe_resolve(config_fvp)

    return first_executable_path(default_fvp_candidates(work_dir))
