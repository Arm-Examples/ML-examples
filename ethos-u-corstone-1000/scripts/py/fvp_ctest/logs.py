#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
# affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
#
"""Log sanitizing and routing for the Corstone-1000 FVP CTest runner."""

from __future__ import annotations

import re

# Terminal output can contain ANSI control sequence introducers (CSI), OSC
# sequences, and single-character escape sequences. Strip them before printing
# application output through CTest so PASS/FAIL regexes see only plain text.
ANSI_RE = re.compile(
    r"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b\][^\a]*\a|\x1b[][()#%*+./0-9A-Za-z]"
)

# Preserve line endings and tabs, but remove other C0 controls that can make
# CTest output hard to read or interfere with regular-expression matching.
CONTROL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")


def sanitize_log(content: str) -> str:
    """
    Remove terminal control sequences from CTest output.

    :param content:     Raw log text.
    :returns:           Sanitized log text.
    """
    return CONTROL_RE.sub("", ANSI_RE.sub("", content))


class ConsoleLogRouter:
    """Route normal-world console text into system and application logs."""

    def __init__(self, normal_log, application_log, test_name: str):
        self.normal_log = normal_log
        self.application_log = application_log
        self.application_start_marker = f"APP_START:{test_name}"
        # The target wrapper prints this once after the application command
        # finishes, so it marks the end of useful application output.
        self.application_end_pattern = re.compile(r"APP_TEST_EXIT:[0-9]+")
        self.log_buffer = ""
        self.in_application_output = False

    def flush(self):
        """Flush pending stream-routing text to the active log."""
        if not self.log_buffer:
            return
        if self.in_application_output:
            self.application_log.write(self.log_buffer)
            self.application_log.flush()
        else:
            self.normal_log.write(self.log_buffer)
            self.normal_log.flush()
        self.log_buffer = ""

    def route(self, text: str):
        """
        Route one chunk of normal-world console text.

        The target-side script emits APP_* markers around the useful test
        section. Everything outside that marked section remains in
        normal-world.log.
        """
        self.log_buffer += text

        while self.log_buffer:
            if not self.in_application_output:
                marker_index = self.log_buffer.find(self.application_start_marker)
                if marker_index == -1:
                    # Retain a marker-sized suffix because APP_* markers can
                    # be split across separate telnet chunks.
                    keep = max(
                        0,
                        len(self.log_buffer) - len(self.application_start_marker),
                    )
                    if keep == 0:
                        return
                    self.normal_log.write(self.log_buffer[:keep])
                    self.normal_log.flush()
                    self.log_buffer = self.log_buffer[keep:]
                    continue

                self.normal_log.write(self.log_buffer[:marker_index])
                self.normal_log.flush()
                self.log_buffer = self.log_buffer[marker_index:]
                self.in_application_output = True
                continue

            end_match = self.application_end_pattern.search(self.log_buffer)
            if not end_match:
                # Retain a marker-sized suffix because APP_* markers can
                # be split across separate telnet chunks.
                keep = max(0, len(self.log_buffer) - len("APP_TEST_EXIT:000"))
                if keep == 0:
                    return
                self.application_log.write(self.log_buffer[:keep])
                self.application_log.flush()
                self.log_buffer = self.log_buffer[keep:]
                continue

            end_index = end_match.end()
            newline_index = self.log_buffer.find("\n", end_index)
            if newline_index != -1:
                end_index = newline_index + 1
            self.application_log.write(self.log_buffer[:end_index])
            self.application_log.flush()
            self.log_buffer = self.log_buffer[end_index:]
            self.in_application_output = False
