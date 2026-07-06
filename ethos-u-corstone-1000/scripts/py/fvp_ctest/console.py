#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
"""Target console handling for the Corstone-1000 FVP CTest runner."""

from __future__ import annotations

import asyncio
import re
import time
from dataclasses import dataclass
from pathlib import Path
from types import TracebackType
from typing import Any, TextIO

import telnetlib3

from fvp_ctest.config import RunnerConfig
from fvp_ctest.logs import ConsoleLogRouter
from fvp_ctest.status import CTestStatus, PROMPT, TARGET_TIMEOUT_SECONDS


@dataclass(frozen=True)
class ConsoleLogs:
    """Open normal-world and application log streams."""

    normal_log: TextIO
    application_log: TextIO


@dataclass(frozen=True)
class ExpectFailure:
    """CTest failure statuses for one expected console literal."""

    timeout_status: int
    timeout_message: str
    eof_status: int
    eof_message: str


async def drain_serial_port(port: str, log_file: Path):
    """
    Drain one FVP serial port to a log file.

    :param port:        TCP port to connect to.
    :param log_file:    Destination log file.
    """
    try:
        reader, writer = await telnetlib3.open_connection(
            "127.0.0.1", int(port), encoding="utf-8", force_binary=False
        )
        with log_file.open("a", encoding="utf-8", errors="replace") as log:
            while True:
                chunk = await reader.read(4096)
                if not chunk:
                    break
                log.write(chunk)
                log.flush()
        writer.close()
    except Exception as error:  # pylint: disable=broad-except
        with log_file.open("a", encoding="utf-8", errors="replace") as log:
            log.write(f"\nserial drain disconnected: {error}\n")


async def run_target_test(normal_port: str, config: RunnerConfig):
    """
    Drive root login, execute the target-side test script, and sync.

    :param normal_port:     Normal-world TCP port.
    :param config:          Runner configuration.
    """
    async with await _open_normal_console(normal_port, config) as console:
        await console.send_line("")
        await console.expect_literal(
            "login:",
            TARGET_TIMEOUT_SECONDS,
            ExpectFailure(
                10,
                "timed out waiting for login prompt",
                11,
                "target disconnected before login prompt",
            ),
        )
        await console.send_line("root")
        await console.expect_prompt(12)

        cd_command = (
            # The packaged MMC mount point varies with kernel/device ordering,
            # so locate the mount that contains this test before executing it.
            "for d in /run/media/mmcblk0 /run/media/mmcblk1 /run/media/*; "
            f'do if test -f "$d/{config.target_test_path}"; '
            'then cd "$d"; fi; done'
        )
        await console.send_line(cd_command)
        await console.expect_prompt(14)

        await console.send_line(
            f"sh ./{config.target_test_path}; "
            r"printf '\nAPP_TEST_EXIT:%s\n' $?"
        )
        await console.expect_app_result()
        await console.expect_prompt(27)

        await console.send_line("sync")
        await console.expect_prompt(40)


async def _open_normal_console(normal_port: str, config: RunnerConfig) -> ConsoleSession:
    """
    Open the normal-world console and log all traffic.

    :param normal_port:     Normal-world TCP port.
    :param config:          Runner configuration.
    :returns:               Console session.
    """
    reader, writer = await telnetlib3.open_connection(
        "127.0.0.1", int(normal_port), encoding="utf-8", force_binary=False
    )
    normal_log = (config.log_dir / "normal-world.log").open(
        "a", encoding="utf-8", errors="replace"
    )
    application_log = (config.log_dir / "application.log").open(
        "a", encoding="utf-8", errors="replace"
    )
    return ConsoleSession(
        reader,
        writer,
        ConsoleLogs(normal_log, application_log),
        config.test_name,
    )


class ConsoleSession:
    """Logged telnet session for the normal-world console."""

    def __init__(
        self,
        reader: Any,
        writer: Any,
        logs: ConsoleLogs,
        test_name: str,
    ):
        self.reader = reader
        self.writer = writer
        self.router = ConsoleLogRouter(logs.normal_log, logs.application_log, test_name)
        self.logs = logs
        self.buffer = ""

    async def __aenter__(self) -> ConsoleSession:
        return self

    async def __aexit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        traceback: TracebackType | None,
    ):
        self.router.flush()
        self.writer.close()
        self.logs.normal_log.close()
        self.logs.application_log.close()

    async def send_line(self, line: str):
        """
        Send one command line to the target shell.

        :param line:    Command line without trailing carriage return.
        """
        self.writer.write(f"{line}\r")
        drain = getattr(self.writer, "drain", None)
        if drain:
            await drain()
        await asyncio.sleep(0.05)

    async def _read_chunk(
        self,
        timeout: float,
        timeout_status: int,
        timeout_message: str,
    ):
        """
        Read one chunk from the target console.

        :param timeout:             Read timeout in seconds.
        :param timeout_status:      Status to raise on timeout.
        :param timeout_message:     Message to raise on timeout.
        :returns:                   Text chunk.
        :raises CTestStatus:        On timeout or disconnect.
        """
        try:
            chunk = await asyncio.wait_for(self.reader.read(4096), timeout)
        except asyncio.TimeoutError as error:
            raise CTestStatus(timeout_status, timeout_message) from error
        if not chunk:
            raise EOFError
        self.buffer += chunk
        self.router.route(chunk)
        return chunk

    async def expect_literal(
        self,
        literal: str,
        timeout: float,
        failure: ExpectFailure,
    ):
        """
        Wait for literal text in the console stream.

        :param literal: Text to wait for.
        :param timeout: Timeout in seconds.
        :param failure: Status values and messages to raise on failure.
        """
        deadline = time.monotonic() + timeout
        while literal not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CTestStatus(failure.timeout_status, failure.timeout_message)
            try:
                await self._read_chunk(
                    remaining,
                    failure.timeout_status,
                    failure.timeout_message,
                )
            except EOFError as error:
                raise CTestStatus(failure.eof_status, failure.eof_message) from error
        self.buffer = self.buffer.split(literal, 1)[1]

    async def expect_prompt(self, status: int):
        """
        Wait for the root shell prompt.

        :param status:      Base status for timeout and disconnect failures.
        """
        await self.expect_literal(
            PROMPT,
            TARGET_TIMEOUT_SECONDS,
            ExpectFailure(
                status,
                "timed out waiting for shell prompt",
                status + 1,
                "target disconnected waiting for shell prompt",
            ),
        )

    async def expect_app_result(self):
        """
        Parse APP_* markers from the target-side test script.

        The target wrapper prints APP_EXIT for the application command and
        APP_TEST_EXIT for the wrapper itself. A non-zero value in either
        marker is a test failure, while APP_MISSING_OUTPUT indicates that the
        wrapper could not find an expected generated output file.
        """
        marker_patterns = [
            # APP_MISSING_OUTPUT:<path>
            (re.compile(r"APP_MISSING_OUTPUT:[^\r\n]+"), 25),
            # APP_EXIT:<test-name>:<non-zero-status>
            (re.compile(r"APP_EXIT:[^:\r\n]+:[1-9][0-9]*"), 24),
            # APP_TEST_EXIT:<non-zero-status>
            (re.compile(r"APP_TEST_EXIT:[1-9][0-9]*"), 24),
            # APP_TEST_EXIT:0
            (re.compile(r"APP_TEST_EXIT:0"), 0),
        ]
        deadline = time.monotonic() + TARGET_TIMEOUT_SECONDS
        while True:
            for pattern, status in marker_patterns:
                if pattern.search(self.buffer):
                    if status == 0:
                        self.buffer = pattern.split(self.buffer, maxsplit=1)[1]
                        return
                    raise CTestStatus(status, pattern.search(self.buffer).group(0))

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CTestStatus(20, "timed out waiting for target test result")
            try:
                await self._read_chunk(
                    remaining, 20, "timed out waiting for target test result"
                )
            except EOFError as error:
                raise CTestStatus(
                    22, "target disconnected waiting for target test result"
                ) from error
