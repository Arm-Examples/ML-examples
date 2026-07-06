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
"""Run one Corstone-1000 FVP-backed CTest from a packaged MMC image."""

from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path

from fvp_ctest.config import parse_runner_arguments
from fvp_ctest.launcher import record_port
from fvp_ctest.status import skip_prerequisite


def _load_runner():
    """
    Import the FVP runner after handling dependency-light callback commands.

    :returns:   FVP runner class.
    """
    try:
        # pylint: disable-next=import-outside-toplevel
        from fvp_ctest.runner import FvpCTestRunner
    except ModuleNotFoundError as error:
        if error.name == "telnetlib3":
            skip_prerequisite(
                "Python package telnetlib3 is not installed in the project "
                "virtual environment. Run setup_model_resources.sh to populate "
                "downloads/env, or configure with -DPYTHON_VENV=<path-to-venv> "
                "that has scripts/py/requirements.txt installed."
            )
        raise
    return FvpCTestRunner


def main(argv: list[str] | None = None):
    """Run the requested entrypoint."""
    argv = sys.argv[1:] if argv is None else argv
    if argv and argv[0] == "record-port":
        parser = argparse.ArgumentParser(description="Record an FVP terminal TCP port.")
        parser.add_argument("output_file", type=Path)
        parser.add_argument("port")
        args = parser.parse_args(argv[1:])
        record_port(args.output_file, args.port)
        return

    config = parse_runner_arguments(argv, Path(__file__).resolve())
    FvpCTestRunner = _load_runner()
    runner = FvpCTestRunner(config)
    raise SystemExit(asyncio.run(runner.run()))


if __name__ == "__main__":
    main()
