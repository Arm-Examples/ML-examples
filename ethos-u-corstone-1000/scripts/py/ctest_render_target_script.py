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
"""Render a target-side shell script from Jinja templates."""

import argparse
import logging
import shlex
from pathlib import Path
from typing import NoReturn

from corstone1000_fvp.util import configure_logging, create_jinja_environment

LOGGER = logging.getLogger(__name__)


def _parse_args() -> argparse.Namespace:
    """
    Parse command line arguments.

    :returns:   Parsed command line arguments.
    """
    parser = argparse.ArgumentParser(
        allow_abbrev=False,
        description="Render one target-side shell script from Jinja templates.",
    )
    parser.add_argument(
        "--templates-dir",
        required=True,
        type=Path,
        help="Directory containing Jinja templates.",
    )
    parser.add_argument(
        "--test-name",
        required=True,
        help="CTest name used in target-side status markers.",
    )
    parser.add_argument(
        "--output-script",
        required=True,
        type=Path,
        help="Rendered target-side shell script path.",
    )
    parser.add_argument(
        "--output-dir",
        default="./outputs",
        help="Target-side output directory to create before running the command.",
    )
    parser.add_argument(
        "--expected-output",
        action="append",
        default=[],
        help="Target-side output file expected after the command exits.",
    )
    parser.add_argument(
        "--command",
        nargs=argparse.REMAINDER,
        required=True,
        help="Target-side command and arguments to run.",
    )
    return parser.parse_args()


def _die(message: str) -> NoReturn:
    """
    Log an error message and terminate the process.

    :param message:     Error message to log.
    :raises SystemExit: Always raised with a failing process status.
    """
    LOGGER.error("ERROR: %s", message)
    raise SystemExit(1)


def _shell_quote(value: str) -> str:
    """
    Quote one shell argument.

    :param value:   Argument value.
    :returns:       Shell-quoted argument.
    """
    return shlex.quote(value)


def _render_script(args: argparse.Namespace) -> str:
    """
    Render the target-side shell script.

    :param args:    Parsed command line arguments.
    :returns:       Rendered shell script content.
    """
    if not args.command:
        _die("--command requires at least one command argument.")

    templates_dir = args.templates_dir.expanduser().resolve()
    try:
        environment = create_jinja_environment(
            templates_dir,
            {"shell_quote": _shell_quote},
        )
    except FileNotFoundError as error:
        _die(str(error))
    template = environment.get_template("run_ctest_one.sh.j2")
    return template.render(
        command=args.command,
        expected_outputs=args.expected_output,
        output_dir=args.output_dir,
        test_name=args.test_name,
    )


def main():
    """Render one target-side FVP CTest script."""
    configure_logging()
    args = _parse_args()
    output_script = args.output_script.expanduser().resolve()
    output_script.parent.mkdir(parents=True, exist_ok=True)
    output_script.write_text(_render_script(args), encoding="utf-8")


if __name__ == "__main__":
    main()
