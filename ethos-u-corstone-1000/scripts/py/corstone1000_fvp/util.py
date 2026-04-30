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
"""Shared utility helpers for Corstone-1000 FVP scripts."""

from __future__ import annotations

from collections.abc import Callable, Mapping
import logging
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

JinjaFilter = Callable[[str], str]


def configure_logging():
    """Configure lightweight console logging for local helper scripts."""
    logging.basicConfig(level=logging.INFO, format="%(message)s")


def create_jinja_environment(
    templates_dir: Path,
    filters: Mapping[str, JinjaFilter] | None = None,
) -> Environment:
    """
    Create a strict Jinja2 environment for project helper templates.

    :param templates_dir:       Template directory.
    :param filters:             Optional custom filters to register.
    :returns:                   Configured Jinja2 environment.
    :raises FileNotFoundError:  If the template directory does not exist.
    """
    if not templates_dir.is_dir():
        raise FileNotFoundError(f"missing template directory: {templates_dir}")

    environment = Environment(
        loader=FileSystemLoader(str(templates_dir)),
        autoescape=False,
        keep_trailing_newline=True,
        trim_blocks=False,
        undefined=StrictUndefined,
    )
    if filters:
        environment.filters.update(filters)
    return environment
