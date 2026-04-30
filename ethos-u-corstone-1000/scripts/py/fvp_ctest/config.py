#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
"""Configuration parsing for the Corstone-1000 FVP CTest runner."""

from __future__ import annotations

import re
from argparse import ArgumentParser
from dataclasses import dataclass
from pathlib import Path

from corstone1000_fvp.discovery import safe_resolve
from fvp_ctest.status import die


@dataclass(frozen=True)
class RunnerConfig:
    """Parsed runner configuration."""

    project_dir: Path
    build_dir: Path
    test_name: str
    source_image: Path
    target_test_path: str
    runner_script: Path

    @property
    def test_name_safe(self) -> str:
        """CTest name made safe for use as a directory name."""
        return re.sub(r"[^A-Za-z0-9_.-]", "_", self.test_name)

    @property
    def run_dir(self) -> Path:
        """Per-test FVP run directory."""
        return self.build_dir / "Testing" / "FVP" / self.test_name_safe

    @property
    def log_dir(self) -> Path:
        """Per-test log directory."""
        return self.run_dir / "logs"

    @property
    def app_image(self) -> Path:
        """Per-test copy of the source MMC image."""
        return self.run_dir / self.source_image.name


def parse_runner_arguments(argv: list[str], runner_script: Path) -> RunnerConfig:
    """
    Parse the runner command line.

    :param argv:            Command line arguments.
    :param runner_script:   Script path used for FVP terminal callbacks.
    :returns:               Runner configuration.
    """
    parser = ArgumentParser(
        prog="ctest_run_fvp.py",
        description="Run one Corstone-1000 FVP-backed CTest.",
        epilog=(
            "Explicit form: <project-dir> <build-dir> <test-name> "
            "<mmc-image> <target-test-path>. Legacy form: <project-dir> "
            "<build-dir> <test-name> <target-test-script>, which assumes "
            "<build-dir>/corstone-1000-mmc.img and tests/<script>."
        ),
    )
    parser.add_argument("project_dir")
    parser.add_argument("build_dir")
    parser.add_argument("test_name")
    parser.add_argument("test_payload", nargs="+")
    args = parser.parse_args(argv)

    if len(args.test_payload) not in (1, 2):
        die("expected four legacy arguments or five explicit arguments")

    project_dir = safe_resolve(args.project_dir)
    build_dir = safe_resolve(args.build_dir)
    test_name = args.test_name
    if len(args.test_payload) == 2:
        source_image = safe_resolve(args.test_payload[0])
        target_test_path = args.test_payload[1]
    else:
        source_image = build_dir / "corstone-1000-mmc.img"
        target_test_path = f"tests/{args.test_payload[0]}"

    if not project_dir.is_dir():
        die(f"required directory is missing: {project_dir}")
    if not build_dir.is_dir():
        die(f"required directory is missing: {build_dir}")

    # The path is sent to the target shell inside a generated command, so keep
    # the accepted character set deliberately small and reject shell metacharacters.
    if not re.fullmatch(r"[A-Za-z0-9_./-]+", target_test_path):
        die(f"invalid target test path: {target_test_path}")
    # The test script must come from the mounted MMC image, not an arbitrary
    # target filesystem location.
    if target_test_path.startswith("/") or ".." in Path(target_test_path).parts:
        die(
            "target test path must be relative and must not contain '..': "
            f"{target_test_path}"
        )

    return RunnerConfig(
        project_dir,
        build_dir,
        test_name,
        source_image,
        target_test_path,
        safe_resolve(runner_script),
    )
