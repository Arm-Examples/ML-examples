#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or
# its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
"""Create a FAT MMC image for Corstone-1000 direct-drive examples."""

import argparse
import glob
import json
import logging
import shlex
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path, PurePosixPath
from typing import Any, NoReturn, Optional

from corstone1000_fvp.util import configure_logging

MMC_IMAGE_SIZE = "64MiB"
LAYOUT_DIRECTORY_KEYS = ("bin", "lib", "models", "inputs", "labels", "outputs", "tests")
DEFAULT_LAYOUT_DIRECTORIES = {
    "bin": "bin",
    "lib": "lib",
    "models": "models",
    "inputs": "inputs",
    "labels": "labels",
    "outputs": "outputs",
    "tests": "tests",
}
LOGGER = logging.getLogger(__name__)
IMAGE_SIZE_SUFFIXES = {
    "": 1,
    "K": 1024,
    "M": 1024 * 1024,
    "G": 1024 * 1024 * 1024,
    "KIB": 1024,
    "MIB": 1024 * 1024,
    "GIB": 1024 * 1024 * 1024,
}
HUMAN_READABLE_SIZE_SUFFIXES = (
    ("GiB", IMAGE_SIZE_SUFFIXES["GIB"]),
    ("MiB", IMAGE_SIZE_SUFFIXES["MIB"]),
    ("KiB", IMAGE_SIZE_SUFFIXES["KIB"]),
)


def _parse_args() -> argparse.Namespace:
    """
    Parse command line arguments.

    :returns:   The parsed command line arguments.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Create or update a FAT MMC image with explicitly typed "
            "runtime files."
        )
    )

    # Locate the MMC image that will receive the packaged files.
    parser.add_argument(
        "--image",
        required=True,
        type=Path,
        help="FAT MMC image to create or update.",
    )
    parser.add_argument(
        "--image-size",
        default=MMC_IMAGE_SIZE,
        help=(
            "MMC image size to create, using bytes or a K/M/G/KiB/MiB/GiB "
            f"suffix. Defaults to {MMC_IMAGE_SIZE}."
        ),
    )
    parser.add_argument(
        "--layout",
        type=Path,
        help="JSON file defining target-side MMC directory names.",
    )

    # Collect files by their target-side runtime directory.
    parser.add_argument(
        "--binary",
        action="append",
        default=[],
        type=Path,
        help="Executable path. May be repeated.",
    )
    parser.add_argument(
        "--model",
        action="append",
        default=[],
        help="Model file or glob copied to the configured model directory.",
    )
    parser.add_argument(
        "--input",
        action="append",
        default=[],
        help="Input file or glob copied to the configured input directory.",
    )
    parser.add_argument(
        "--label",
        action="append",
        default=[],
        help="Labels file or glob copied to the configured labels directory.",
    )
    parser.add_argument(
        "--library",
        action="append",
        default=[],
        type=Path,
        help="Shared library path. May be repeated.",
    )
    parser.add_argument(
        "--test",
        action="append",
        default=[],
        type=Path,
        help="Target-side test file copied to the configured tests directory.",
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


def _validate_layout_directory(role: str, image_dir: str) -> str:
    """
    Validate one target-side directory from the MMC layout.

    :param role:       Directory role name from the layout file.
    :param image_dir:  Target-side directory path.
    :returns:          Validated directory path.
    """
    path = PurePosixPath(image_dir)
    if (
        not image_dir
        or image_dir.startswith("/")
        or "\\" in image_dir
        or path.parts in ((), (".",))
        or ".." in path.parts
    ):
        _die(f"Invalid MMC layout directory for {role}: {image_dir}")

    return image_dir.strip("/")


def _require_layout_mapping(layout_data: Any) -> dict[str, Any]:
    """
    Validate that parsed layout data contains a directory mapping.

    :param layout_data:    Parsed JSON data.
    :returns:             Directory mapping from the layout.
    """
    if not isinstance(layout_data, dict):
        _die("MMC layout JSON must contain an object at the top level.")

    directories = layout_data.get("directories")
    if not isinstance(directories, dict):
        _die("MMC layout JSON must contain a directories object.")

    return directories


def _load_layout_directories(layout_path: Optional[Path]) -> dict[str, str]:
    """
    Load the target-side MMC directory layout.

    :param layout_path:    Optional JSON layout file path.
    :returns:             Directory names keyed by packaging role.
    """
    if layout_path is None:
        return dict(DEFAULT_LAYOUT_DIRECTORIES)

    resolved_layout_path = layout_path.expanduser().resolve()
    if not resolved_layout_path.is_file():
        _die(f"MMC layout file not found: {resolved_layout_path}")

    try:
        layout_data = json.loads(resolved_layout_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        _die(f"Invalid MMC layout JSON in {resolved_layout_path}: {error}")

    directories = _require_layout_mapping(layout_data)
    layout_directories: dict[str, str] = {}
    used_directories: set[str] = set()
    for role in LAYOUT_DIRECTORY_KEYS:
        image_dir = directories.get(role)
        if not isinstance(image_dir, str):
            _die(f"MMC layout JSON must define directories.{role}.")
        layout_directory = _validate_layout_directory(role, image_dir)
        if layout_directory in used_directories:
            _die(f"Duplicate MMC layout directory: {layout_directory}")
        used_directories.add(layout_directory)
        layout_directories[role] = layout_directory

    return layout_directories


def _get_image_dirs(layout_directories: dict[str, str]) -> list[str]:
    """
    Get the ordered target-side directories that must exist in the image.

    :param layout_directories:  Directory names keyed by packaging role.
    :returns:                   Unique directory names in layout order.
    """
    image_dirs: list[str] = []
    for role in LAYOUT_DIRECTORY_KEYS:
        image_dir = layout_directories[role]
        if image_dir not in image_dirs:
            image_dirs.append(image_dir)

    return image_dirs


def _format_command(command: list[str]) -> str:
    """
    Format a command for diagnostic output.

    :param command: Command argument vector.
    :returns:       Shell-escaped command text.
    """
    return shlex.join(command)


def _log_command_output(result: subprocess.CompletedProcess[str]):
    """
    Log captured subprocess output to the matching log levels.

    :param result:  Completed subprocess result with text output.
    """
    if result.stdout:
        LOGGER.info("%s", result.stdout.rstrip())
    if result.stderr:
        LOGGER.error("%s", result.stderr.rstrip())


def _require_command(command: str):
    """
    Require a host command to be discoverable on PATH.

    :param command: Command name to locate.
    """
    if shutil.which(command) is None:
        _die(f"{command} not found")


def _run(
    command: list[str],
    *,
    quiet: bool = False,
    allow_failure: bool = False,
) -> subprocess.CompletedProcess[str]:
    """
    Run a host command and optionally allow non-zero exit status.

    :param command:         Command argument vector.
    :param quiet:           Suppress successful command output when true.
    :param allow_failure:   Return non-zero results instead of exiting.
    :returns:               Completed subprocess result.
    """
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        timeout=None,
    )

    if result.returncode == 0:
        if not quiet:
            _log_command_output(result)
        return result

    if allow_failure:
        return result

    _log_command_output(result)
    _die(
        "Command failed with exit code "
        f"{result.returncode}: {_format_command(command)}"
    )


def _image_dir_exists(image_path: Path, image_dir: str) -> bool:
    """
    Check whether a target directory exists in the MMC image.

    :param image_path:  MMC image path.
    :param image_dir:   Target-side directory name.
    :returns:           True when the directory exists.
    """
    result = _run(
        ["mdir", "-i", str(image_path), f"::/{image_dir}"],
        quiet=True,
        allow_failure=True,
    )
    return result.returncode == 0


def _is_not_found_result(result: subprocess.CompletedProcess[str]) -> bool:
    """
    Check whether a failed mtools result reports a missing file.

    :param result:  Completed subprocess result.
    :returns:       True when the output reports a missing file.
    """
    output = f"{result.stdout}\n{result.stderr}".lower()
    return "not found" in output


def _parse_image_size_bytes(image_size: str) -> int:
    """
    Parse an MMC image size string into bytes.

    :param image_size:  Size text using bytes or a K/M/G/KiB/MiB/GiB suffix.
    :returns:           Parsed size in bytes.
    """
    stripped_image_size = image_size.strip()
    digit_count = 0
    while (
        digit_count < len(stripped_image_size)
        and stripped_image_size[digit_count].isdigit()
    ):
        digit_count += 1

    if digit_count == 0:
        _die(f"Invalid MMC image size: {image_size}")

    size_value_text = stripped_image_size[:digit_count]
    size_suffix = stripped_image_size[digit_count:].upper()
    if size_suffix not in IMAGE_SIZE_SUFFIXES:
        _die(f"Invalid MMC image size suffix: {image_size}")

    size_bytes = int(size_value_text) * IMAGE_SIZE_SUFFIXES[size_suffix]
    if size_bytes <= 0:
        _die(f"Invalid MMC image size: {image_size}")

    return size_bytes


def _format_size(size_bytes: int) -> str:
    """
    Format a file size using binary units.

    :param size_bytes:  File size in bytes.
    :returns:           Human-readable file size.
    """
    if size_bytes < IMAGE_SIZE_SUFFIXES["KIB"]:
        suffix = "byte" if size_bytes == 1 else "bytes"
        return f"{size_bytes} {suffix}"

    suffix_index = 0
    while (
        suffix_index < len(HUMAN_READABLE_SIZE_SUFFIXES) - 1
        and size_bytes < HUMAN_READABLE_SIZE_SUFFIXES[suffix_index][1]
    ):
        suffix_index += 1

    suffix, multiplier = HUMAN_READABLE_SIZE_SUFFIXES[suffix_index]
    formatted_size = size_bytes / multiplier
    if size_bytes % multiplier == 0:
        return f"{formatted_size:.0f} {suffix}"

    trimmed_size = f"{formatted_size:.3f}".rstrip("0").rstrip(".")
    return f"{trimmed_size} {suffix}"


def _resolve_file_specs(specs: list[str], image_dir: str) -> list[Path]:
    """
    Resolve exact file paths and expand glob patterns.

    :param specs:       File paths or glob patterns to resolve.
    :param image_dir:   Target-side directory used in error messages.
    :returns:           Resolved file paths.
    """
    files: list[Path] = []
    for spec in specs:
        expanded_spec = str(Path(spec).expanduser())
        if glob.has_magic(expanded_spec):
            matches = sorted(
                Path(match).resolve() for match in glob.glob(expanded_spec)
            )
            matches = [match for match in matches if match.is_file()]
            if not matches:
                _die(f"No files matched {image_dir.rstrip('s')} glob: {spec}")
            files.extend(matches)
        else:
            files.append(Path(expanded_spec).resolve())
    return files


def _resolve_file_paths(paths: list[Path]) -> list[Path]:
    """
    Resolve explicit file paths.

    :param paths:   File paths to resolve.
    :returns:       Resolved file paths.
    """
    return [path.expanduser().resolve() for path in paths]


def _resolve_operation_paths(args: argparse.Namespace) -> tuple[Path, str]:
    """
    Resolve and validate top-level image operation paths.

    :param args:    Parsed command line arguments.
    :returns:       Output image and image size.
    """
    image_path = args.image.expanduser().resolve()

    return image_path, args.image_size


def _build_file_manifest(
    args: argparse.Namespace,
    layout_directories: dict[str, str],
) -> dict[str, list[Path]]:
    """
    Build the target-side file manifest from command line arguments.

    :param args:                Parsed command line arguments.
    :param layout_directories:  Directory names keyed by packaging role.
    :returns:                   Source files grouped by target-side directory.
    """
    files_by_dir = {
        layout_directories["bin"]: _resolve_file_paths(args.binary),
        layout_directories["lib"]: _resolve_file_paths(args.library),
        layout_directories["models"]: _resolve_file_specs(
            args.model,
            layout_directories["models"],
        ),
        layout_directories["inputs"]: _resolve_file_specs(
            args.input,
            layout_directories["inputs"],
        ),
        layout_directories["labels"]: _resolve_file_specs(
            args.label,
            layout_directories["labels"],
        ),
        layout_directories["tests"]: [
            path.expanduser().resolve() for path in args.test
        ],
    }

    return files_by_dir


def _validate_files(files_by_dir: dict[str, list[Path]]):
    """
    Validate source files and reject duplicate target-side file names.

    :param files_by_dir:    Source files grouped by target-side directory.
    """
    destinations: dict[str, set[str]] = defaultdict(set)

    for image_dir, files in files_by_dir.items():
        for file_path in files:
            if not file_path.is_file():
                _die(f"Missing {image_dir.rstrip('s')} file: {file_path}")
            if file_path.name in destinations[image_dir]:
                _die(f"Duplicate destination in /{image_dir}: {file_path.name}")
            destinations[image_dir].add(file_path.name)


def _get_total_file_size_bytes(files_by_dir: dict[str, list[Path]]) -> int:
    """
    Get the total size of all source files to package.

    :param files_by_dir:    Source files grouped by target-side directory.
    :returns:               Total source file size in bytes.
    """
    total_size_bytes = 0
    for files in files_by_dir.values():
        for file_path in files:
            total_size_bytes += file_path.stat().st_size

    return total_size_bytes


def _validate_package_size(image_size: str, files_by_dir: dict[str, list[Path]]):
    """
    Validate that source files do not exceed the requested image size.

    :param image_size:      Requested MMC image size.
    :param files_by_dir:    Source files grouped by target-side directory.
    """
    image_size_bytes = _parse_image_size_bytes(image_size)
    total_size_bytes = _get_total_file_size_bytes(files_by_dir)
    if total_size_bytes > image_size_bytes:
        _die(
            "Files to package exceed requested MMC image size: "
            f"{_format_size(total_size_bytes)} > {_format_size(image_size_bytes)}"
        )


def _ensure_mmc_image(image_path: Path, image_size: str):
    """
    Create the MMC image when it is missing or its size has changed.

    :param image_path:  MMC image path.
    :param image_size:  Requested MMC image size.
    """
    image_size_bytes = _parse_image_size_bytes(image_size)

    if image_path.is_file():
        current_size_bytes = image_path.stat().st_size
        if current_size_bytes == image_size_bytes:
            LOGGER.info(
                "Using existing MMC image: %s (%s)",
                image_path,
                _format_size(current_size_bytes),
            )
            return

        LOGGER.info(
            "Recreating MMC image: %s (%s -> %s)",
            image_path,
            _format_size(current_size_bytes),
            _format_size(image_size_bytes),
        )
    else:
        LOGGER.info("Creating MMC image: %s (%s)", image_path, image_size)

    _require_command("truncate")
    _require_command("mkfs.vfat")
    image_path.parent.mkdir(parents=True, exist_ok=True)
    _run(["truncate", "-s", str(image_size_bytes), str(image_path)])
    _run(["mkfs.vfat", "-F", "32", str(image_path)], quiet=True)


def _create_image_dirs(image_path: Path, image_dirs: list[str]):
    """
    Ensure the expected target-side directories exist in the MMC image.

    :param image_path:  MMC image path.
    :param image_dirs:  Target-side directories to create.
    """
    for image_dir in image_dirs:
        if not _image_dir_exists(image_path, image_dir):
            _run(["mmd", "-i", str(image_path), f"::/{image_dir}"], quiet=True)


def _clear_image_dirs(image_path: Path, image_dirs: list[str]):
    """
    Remove packaged files from the expected target-side directories.

    :param image_path:  MMC image path.
    :param image_dirs:  Target-side directories to clear.
    """
    for image_dir in image_dirs:
        result = _run(
            ["mdel", "-i", str(image_path), f"::/{image_dir}/*"],
            quiet=True,
            allow_failure=True,
        )
        if result.returncode != 0 and not _is_not_found_result(result):
            _log_command_output(result)
            _die(
                "Command failed while clearing MMC image directory "
                f"/{image_dir}: {_format_command(result.args)}"
            )


def _copy_to_image_dir(image_path: Path, image_dir: str, files: list[Path]):
    """
    Copy host files into one target-side directory in the MMC image.

    :param image_path:  MMC image path.
    :param image_dir:   Target-side directory name.
    :param files:       Host files to copy.
    """
    for file_path in files:
        LOGGER.info("Copying to /%s: %s", image_dir, file_path)
        _run(["mcopy", "-o", "-i", str(image_path), str(file_path), f"::/{image_dir}/"])


def _log_image_contents(image_path: Path, image_dirs: list[str]):
    """
    Log the final contents of the expected target-side directories.

    :param image_path:  MMC image path.
    :param image_dirs:  Target-side directories to list.
    """
    LOGGER.info("")
    LOGGER.info("MMC image contents:")
    for image_dir in image_dirs:
        _run(["mdir", "-i", str(image_path), "-/", f"::/{image_dir}"])


def _log_file_manifest(
    image_path: Path,
    image_size: str,
    files_by_dir: dict[str, list[Path]],
):
    """
    Log the files that will be packaged into the MMC image.

    :param image_path:      MMC image path.
    :param image_size:      Requested MMC image size.
    :param files_by_dir:    Source files grouped by target-side directory.
    """
    LOGGER.info("Preparing Corstone-1000 MMC image")
    LOGGER.info("Image path:      %s", image_path)
    LOGGER.info("Image size:      %s", image_size)
    LOGGER.info("Files to package:")
    for image_dir, files in files_by_dir.items():
        LOGGER.info("  /%s: %d file(s)", image_dir, len(files))
        for file_path in files:
            LOGGER.info(
                "    %s (%s)",
                file_path,
                _format_size(file_path.stat().st_size),
            )
    LOGGER.info(
        "Total source file size: %s",
        _format_size(_get_total_file_size_bytes(files_by_dir)),
    )


def _log_python_invocation():
    """Log a shell-copyable command for this Python invocation."""
    LOGGER.info("Python invocation: %s", shlex.join([sys.executable, *sys.argv]))


def _require_mtools():
    """
    Require the host mtools commands needed to package the image.
    """
    for command in ("mcopy", "mdel", "mmd", "mdir"):
        _require_command(command)


def _prepare_mmc_image(image_path: Path, image_size: str, image_dirs: list[str]):
    """
    Prepare the output MMC image before copying files into it.

    :param image_path:  MMC image path.
    :param image_size:  Requested MMC image size.
    :param image_dirs:  Target-side directories to prepare.
    """
    _ensure_mmc_image(image_path, image_size)
    _create_image_dirs(image_path, image_dirs)
    _clear_image_dirs(image_path, image_dirs)


def main():
    """Run the command line MMC packaging flow."""
    configure_logging()
    args = _parse_args()

    image_path, image_size = _resolve_operation_paths(args)
    layout_directories = _load_layout_directories(args.layout)
    image_dirs = _get_image_dirs(layout_directories)
    files_by_dir = _build_file_manifest(
        args,
        layout_directories,
    )
    _log_python_invocation()
    _validate_files(files_by_dir)
    _log_file_manifest(image_path, image_size, files_by_dir)
    _validate_package_size(image_size, files_by_dir)

    _require_mtools()
    _prepare_mmc_image(image_path, image_size, image_dirs)

    for image_dir, files in files_by_dir.items():
        _copy_to_image_dir(image_path, image_dir, files)

    _log_image_contents(image_path, image_dirs)
    LOGGER.info("")
    LOGGER.info("MMC image updated: %s", image_path)


if __name__ == "__main__":
    main()
