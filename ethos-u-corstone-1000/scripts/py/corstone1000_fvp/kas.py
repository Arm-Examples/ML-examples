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
"""kas wrapper helpers for manual Corstone-1000 FVP launches."""

from __future__ import annotations

import json
import os
import shlex
import shutil
import tempfile
from pathlib import Path

from corstone1000_fvp.discovery import safe_resolve
from corstone1000_fvp.util import create_jinja_environment

KAS_CONFIG_FILES = (
    ("meta-arm", "kas/corstone1000-fvp.yml"),
    ("meta-arm", "ci/debug.yml"),
    ("meta-arm", "kas/corstone1000-a320.yml"),
)
TEMPLATE_DIR = Path(__file__).resolve().parent / "templates"
TEMPLATE_NAME = "kas_top_config.yml.j2"


class KasError(RuntimeError):
    """Failure to resolve or prepare kas launch resources."""


def resolve_command(
    explicit: str | None,
    fallback: Path,
    description: str,
) -> Path:
    """
    Resolve a command from an explicit value, fallback path, or ``PATH``.

    :param explicit:        Explicit command name or path.
    :param fallback:        Fallback executable path.
    :param description:     Human-readable command description.
    :returns:               Resolved command path.
    :raises KasError:       If the command cannot be resolved.
    """
    if explicit:
        if "/" in explicit:
            candidate = safe_resolve(explicit)
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
            raise KasError(f"{description} not found or not executable: {candidate}")

        command_path = shutil.which(explicit)
        if command_path:
            return Path(command_path)
        raise KasError(f"{description} not found on PATH: {explicit}")

    if fallback.is_file() and os.access(fallback, os.X_OK):
        return safe_resolve(fallback)

    command_path = shutil.which("kas")
    if command_path:
        return Path(command_path)

    raise KasError(
        "kas not found. Use --kas, KAS, install project Python requirements "
        "for kas==4.4, or add kas to PATH."
    )


def meta_arm_path(work_dir: Path) -> Path:
    """
    Return the meta-arm repository path for a workspace.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :returns:           meta-arm repository path.
    """
    return work_dir / "meta-arm"


def validate_kas_inputs(work_dir: Path):
    """
    Validate the kas fragments needed for manual FVP launch.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :raises KasError:   If required fragments are missing.
    """
    meta_arm = meta_arm_path(work_dir)
    if not meta_arm.is_dir():
        raise KasError(f"meta-arm repository not found at {meta_arm}")

    for repository, relative_path in KAS_CONFIG_FILES:
        if repository != "meta-arm":
            raise KasError(f"unknown kas config repo: {repository}")
        config_path = meta_arm / relative_path
        if not config_path.is_file():
            raise KasError(f"missing kas config: {config_path}")


def _yaml_quote(value: str) -> str:
    """
    Quote a string as a YAML scalar.

    :param value:   String value.
    :returns:       JSON-compatible quoted string accepted by YAML.
    """
    return json.dumps(value)


def render_top_config(work_dir: Path) -> str:
    """
    Render the temporary top-level kas configuration.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :returns:           Rendered kas configuration content.
    """
    environment = create_jinja_environment(
        TEMPLATE_DIR,
        {"yaml_quote": _yaml_quote},
    )
    template = environment.get_template(TEMPLATE_NAME)
    return template.render(meta_arm_path=str(meta_arm_path(work_dir)))


def write_top_config(work_dir: Path) -> Path:
    """
    Write the temporary top-level kas configuration.

    :param work_dir:    Corstone-1000 Yocto workspace path.
    :returns:           Temporary kas configuration path.
    """
    with tempfile.NamedTemporaryFile(
        "w",
        delete=False,
        encoding="utf-8",
        prefix="corstone1000-fvp-kas.",
        suffix=".yml",
    ) as config_file:
        config_file.write(render_top_config(work_dir))
        return Path(config_file.name)


def build_kas_shell_command(
    kas_path: Path,
    top_config: Path,
    runfvp_command: list[str],
) -> list[str]:
    """
    Build the ``kas shell`` command used for manual FVP launches.

    :param kas_path:         Resolved kas executable path.
    :param top_config:       Temporary top-level kas configuration path.
    :param runfvp_command:   ``runfvp`` command to run inside kas.
    :returns:                Complete kas command argument vector.
    """
    return [str(kas_path), "shell", str(top_config), "-c", shlex.join(runfvp_command)]
