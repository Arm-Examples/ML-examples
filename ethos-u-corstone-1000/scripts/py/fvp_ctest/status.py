#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
"""CTest status helpers for the Corstone-1000 FVP runner."""

from __future__ import annotations

import sys
from typing import NoReturn

SKIP_PREREQUISITE_STATUS = 77
PROMPT = "# "
TARGET_TIMEOUT_SECONDS = 900
PORT_TIMEOUT_SECONDS = 180


class CTestStatus(Exception):
    """Failure with the process status CTest should observe."""

    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message


def skip_prerequisite(message: str) -> NoReturn:
    """
    Print a prerequisite message and exit with the configured CTest skip code.

    :param message:     Missing prerequisite message.
    :raises SystemExit: Always raised with the skip status.
    """
    print(f"FVP CTest prerequisites missing: {message}", file=sys.stderr)
    raise SystemExit(SKIP_PREREQUISITE_STATUS)


def die(message: str) -> NoReturn:
    """
    Print an error message and exit with a failing status.

    :param message:     Error message.
    :raises SystemExit: Always raised with failing status.
    """
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)
