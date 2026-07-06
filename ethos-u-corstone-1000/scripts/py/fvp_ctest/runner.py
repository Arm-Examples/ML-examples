#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
"""High-level orchestration for one Corstone-1000 FVP CTest."""

from __future__ import annotations

import asyncio
import re
import shutil
import subprocess
import time

from fvp_ctest.config import RunnerConfig
from fvp_ctest.console import drain_serial_port, run_target_test
from fvp_ctest.launcher import detect_launcher, start_fvp, stop_fvp
from fvp_ctest.logs import sanitize_log
from fvp_ctest.status import CTestStatus, PORT_TIMEOUT_SECONDS, die


class FvpCTestRunner:
    """Host-side runner for one FVP-backed CTest."""

    def __init__(self, config: RunnerConfig):
        self.config = config
        self.fvp_process: subprocess.Popen | None = None
        self.fvp_log_handle = None
        self.normal_port = ""
        self.secure_port = ""
        self.secure_enclave_port = ""
        self.drain_tasks: list[asyncio.Task] = []

    def prepare_app_image(self):
        """Create the per-test run directory and copy the shared MMC image."""
        if not self.config.source_image.is_file():
            die(f"required file is missing: {self.config.source_image}")

        self.config.log_dir.mkdir(parents=True, exist_ok=True)
        self.config.run_dir.mkdir(parents=True, exist_ok=True)
        for log_name in (
            "application.log",
            "fvp.log",
            "normal-world.log",
            "secure-world.log",
            "secure-enclave.log",
            "host-terminal-0.port",
            "host-terminal-1.port",
            "secure-enclave.port",
        ):
            (self.config.log_dir / log_name).write_text("", encoding="utf-8")
        (self.config.run_dir / "combined.log").write_text("", encoding="utf-8")
        shutil.copy2(self.config.source_image, self.config.app_image)

    async def wait_for_terminal_port_file(self, port_file) -> str:
        """
        Wait until one FVP terminal callback records a TCP port.

        :param port_file:   Port file path.
        :returns:           Recorded TCP port.
        :raises CTestStatus: If FVP exits or the port is not recorded in time.
        """
        deadline = time.monotonic() + PORT_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            try:
                port = port_file.read_text(encoding="utf-8").splitlines()[0].strip()
            except (FileNotFoundError, IndexError):
                port = ""
            if re.fullmatch(r"[0-9]+", port):
                return port

            if self.fvp_process and self.fvp_process.poll() is not None:
                raise CTestStatus(
                    1, f"FVP exited before terminal port was recorded: {port_file}"
                )
            await asyncio.sleep(1)

        raise CTestStatus(1, f"timed out waiting for terminal port file: {port_file}")

    async def start_serial_drains(self):
        """Wait for FVP terminal ports and start non-login console drains."""
        self.normal_port = await self.wait_for_terminal_port_file(
            self.config.log_dir / "host-terminal-0.port"
        )
        self.secure_port = await self.wait_for_terminal_port_file(
            self.config.log_dir / "host-terminal-1.port"
        )
        self.secure_enclave_port = await self.wait_for_terminal_port_file(
            self.config.log_dir / "secure-enclave.port"
        )
        # Secure-world and secure-enclave consoles are passive logs while the
        # normal-world console is driven interactively by run_target_test().
        self.drain_tasks = [
            asyncio.create_task(
                drain_serial_port(
                    self.secure_port,
                    self.config.log_dir / "secure-world.log",
                )
            ),
            asyncio.create_task(
                drain_serial_port(
                    self.secure_enclave_port,
                    self.config.log_dir / "secure-enclave.log",
                )
            ),
        ]

    async def cleanup(self):
        """Stop background serial drains and the FVP process group."""
        for task in self.drain_tasks:
            task.cancel()
        if self.drain_tasks:
            await asyncio.gather(*self.drain_tasks, return_exceptions=True)
        stop_fvp(self.fvp_process, self.fvp_log_handle)

    def collect_outputs(self):
        """Combine logs and print the most useful CTest output."""
        combined = self.config.run_dir / "combined.log"
        pieces = []
        for log_name in (
            "application.log",
            "normal-world.log",
            "fvp.log",
            "secure-world.log",
            "secure-enclave.log",
        ):
            log_file = self.config.log_dir / log_name
            if log_file.is_file():
                pieces.append(f"===== {log_name} =====\n")
                pieces.append(log_file.read_text(encoding="utf-8", errors="replace"))
                pieces.append("\n")
        combined.write_text("".join(pieces), encoding="utf-8")

        application_log = self.config.log_dir / "application.log"
        application_output = ""
        if application_log.is_file():
            application_output = application_log.read_text(
                encoding="utf-8", errors="replace"
            )

        if application_output:
            print(sanitize_log(application_output), end="")
        else:
            print("No application output captured from normal-world console.")
        print(f"\nFull FVP logs: {self.config.log_dir}")

    async def run(self) -> int:
        """
        Run one FVP-backed CTest.

        :returns:   Process status.
        """
        launcher = detect_launcher()
        self.prepare_app_image()
        self.fvp_process, self.fvp_log_handle = start_fvp(self.config, launcher)

        status = 0
        try:
            await self.start_serial_drains()
            await run_target_test(self.normal_port, self.config)
        except CTestStatus as error:
            status = error.status
            with (self.config.log_dir / "normal-world.log").open(
                "a", encoding="utf-8", errors="replace"
            ) as log:
                log.write(f"\n{error.message}\n")
        finally:
            await self.cleanup()
            self.collect_outputs()

        return status
