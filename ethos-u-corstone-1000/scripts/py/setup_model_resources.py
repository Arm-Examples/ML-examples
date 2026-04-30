#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or
# its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
"""Set up model resources for the Corstone-1000 direct-drive examples."""
# pylint: disable=import-outside-toplevel
import argparse
import json
import logging
from pathlib import Path
from typing import Any

VELA_VERSION = "5.0.0"
VELA_URL = "https://git.gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-vela.git"
MIN_PYTHON_VERSION = (3, 10)

_CURRENT_FILE_DIR = Path(__file__).parent.resolve()
_PROJECT_DIR = _CURRENT_FILE_DIR.parents[1]

DEFAULT_DOWNLOADS_DIR = _PROJECT_DIR / "downloads"
DEFAULT_LOG_FILE = Path("log_setup_model_resources.log")
DEFAULT_MANIFEST = _PROJECT_DIR / "resources" / "model_manifest.json"
DEFAULT_VELA_CONFIG = _PROJECT_DIR / "resources" / "vela_corstone_1000.ini"
LOG_LEVELS = {
    "CRITICAL": logging.CRITICAL,
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG,
}
LOG_LEVEL_NAMES = sorted(LOG_LEVELS)


def _parse_log_level(level: str) -> int:
    """
    Parse a named logging level.

    :param level:    Logging level name.
    :return:         Logging level value.
    """
    try:
        return LOG_LEVELS[level.upper()]
    except KeyError as error:
        choices = ", ".join(LOG_LEVEL_NAMES)
        raise argparse.ArgumentTypeError(
            f"invalid log level: {level!r}. Choose from: {choices}"
        ) from error


def _parse_args() -> argparse.Namespace:
    """
    Parse command line arguments.

    :return:    The parsed arguments.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Download and optimise model resources for Corstone-1000 "
            "direct-drive examples."
        )
    )
    log_level_group = parser.add_mutually_exclusive_group()
    log_level_group.add_argument(
        "--log-level",
        help=(
            "Minimum log level emitted to the console. "
            f"Valid values: {LOG_LEVEL_NAMES}"
        ),
        type=_parse_log_level,
        default=LOG_LEVELS["INFO"],
        metavar="LEVEL",
    )
    log_level_group.add_argument(
        "--verbose",
        help="Enable DEBUG logging on the console.",
        action="store_const",
        const=LOG_LEVELS["DEBUG"],
        dest="log_level",
    )
    parser.add_argument(
        "--skip-vela",
        help="Download resources but do not run Vela.",
        action="store_true",
    )
    parser.add_argument(
        "--use-case",
        help=(
            "Only set up resources for the specified use case. "
            "May be specified more than once."
        ),
        action="append",
        default=[],
    )
    parser.add_argument(
        "--additional-ethos-u-config-name",
        help="Additional Ethos-U Vela configuration name to optimise for.",
        action="append",
        default=[],
    )
    parser.add_argument(
        "--arena-cache-size",
        help="Arena cache size in bytes. Zero keeps the MLEK default.",
        type=int,
        default=0,
    )
    parser.add_argument(
        "--clean",
        help=(
            "Check existing resource metadata and remove stale resource "
            "directories before setup."
        ),
        action="store_true",
    )
    parser.add_argument(
        "--parallel",
        help="Number of worker threads used for downloads and optimisation.",
        type=int,
        default=1,
    )
    parser.add_argument(
        "--downloads-dir",
        help="Directory for downloaded and optimised model resources.",
        type=Path,
        default=DEFAULT_DOWNLOADS_DIR,
    )
    parser.add_argument(
        "--venv-dir",
        help="Virtual environment directory. Defaults to <downloads-dir>/env.",
        type=Path,
    )
    parser.add_argument(
        "--model-manifest",
        help="Model resource manifest consumed by mlek-tools.",
        type=Path,
        default=DEFAULT_MANIFEST,
        dest="use_case_resources_file",
    )
    parser.add_argument(
        "--vela-config-file",
        help="Vela configuration file.",
        type=Path,
        default=DEFAULT_VELA_CONFIG,
    )
    return parser.parse_args()


def _validate_args(args: argparse.Namespace):
    """
    Validate parsed command line arguments.

    :param args:    Parsed command line arguments.
    """
    if args.arena_cache_size < 0:
        raise ValueError("Arena cache size cannot be less than 0.")
    if args.parallel < 1:
        raise ValueError("Parallel worker count must be at least 1.")
    if args.venv_dir is None:
        args.venv_dir = args.downloads_dir / "env"
    if not args.use_case_resources_file.is_file():
        raise FileNotFoundError(
            f"Invalid resource manifest: {args.use_case_resources_file}"
        )
    if not args.vela_config_file.is_file():
        raise FileNotFoundError(f"Invalid Vela config file: {args.vela_config_file}")


def _create_default_npu_configs() -> Any:
    """
    Create the default Ethos-U NPU configuration set for MLEK.

    :returns:    MLEK NPU configuration collection.
    """
    from mlek_tools.config.npu import NpuConfigs, valid_npu_configs

    return NpuConfigs.create(valid_npu_configs.get("ethos-u85", 2048))


def _create_optimizer_config(args: argparse.Namespace) -> Any:
    """
    Create the MLEK optimizer configuration.

    :param args:    Parsed command line arguments.
    :returns:       MLEK optimizer configuration.
    """
    from mlek_tools.config.optimizer import OptimizerConfig

    return OptimizerConfig(
        vela_config_file=args.vela_config_file.resolve(),
        additional_npu_config_names=args.additional_ethos_u_config_name,
        arena_cache_size=args.arena_cache_size,
        cop_format="COP2",
        separate_io_regions=True,
    )


def _create_paths_config(args: argparse.Namespace) -> Any:
    """
    Create the MLEK paths configuration.

    :param args:    Parsed command line arguments.
    :returns:       MLEK paths configuration.
    """
    from mlek_tools.config.paths import PathsConfig

    return PathsConfig(
        use_case_resources_files=[args.use_case_resources_file.resolve()],
        downloads_dir=args.downloads_dir.resolve(),
        requirements_files=[],
    )


def _create_tflite_config(args: argparse.Namespace) -> Any:
    """
    Create the MLEK TensorFlow Lite resource setup configuration.

    :param args:    Parsed command line arguments.
    :returns:       MLEK TensorFlow Lite configuration.
    """
    from mlek_tools.config.tflite import TfliteConfig

    return TfliteConfig(
        enabled=True,
        run_vela=not args.skip_vela,
        vela_version=VELA_VERSION,
        vela_url=VELA_URL,
        vela_install_from_source=False,
    )


def _create_executorch_config() -> Any:
    """
    Create the disabled MLEK ExecuTorch setup configuration.

    :returns:    MLEK ExecuTorch configuration.
    """
    from mlek_tools import ExecuTorchConfig

    return ExecuTorchConfig(enabled=False)


def _create_download_config(args: argparse.Namespace) -> Any:
    """
    Create the MLEK download configuration.

    :param args:    Parsed command line arguments.
    :returns:       MLEK download configuration.
    """
    from mlek_tools import DownloadConfig

    return DownloadConfig(
        use_case_names=args.use_case,
        check_clean_folder=args.clean,
        parallel=args.parallel,
    )


def _get_setup_script_hash() -> str:
    """
    Compute the resource setup script hash expected by MLEK.

    :returns:    SHA-256 hash of this script.
    """
    from mlek_tools.setup.util import get_sha256sum_for_file

    return get_sha256sum_for_file(Path(__file__).resolve())


def _set_up_resources(args: argparse.Namespace) -> Path:
    """
    Run MLEK's resource setup flow with direct-drive project defaults.

    :param args:    Parsed command line arguments.
    :return:        The virtual environment directory.
    """
    from mlek_tools.orchestrate import set_up_resources

    return set_up_resources(
        download_config=_create_download_config(args),
        optimizer_config=_create_optimizer_config(args),
        tflite_config=_create_tflite_config(args),
        executorch_config=_create_executorch_config(),
        paths_config=_create_paths_config(args),
        setup_script_hash=_get_setup_script_hash(),
        default_npu_configs=_create_default_npu_configs(),
        default_downloads_path=DEFAULT_DOWNLOADS_DIR,
        min_python_version=MIN_PYTHON_VERSION,
    )


def _write_generated_resource(
    downloads_dir: Path,
    use_case: str,
    resource: dict,
) -> None:
    """
    Materialise a generated resource declared in the model manifest.

    :param downloads_dir:    Directory containing all prepared resources.
    :param use_case:         Name of the resource use case.
    :param resource:         Generated resource manifest entry.
    """
    resource_type = resource.get("type")
    if resource_type != "zero_binary":
        raise ValueError(f"Unsupported generated resource type: {resource_type}")

    name = resource.get("name")
    if not name or Path(name).name != name:
        raise ValueError(f"Invalid generated resource name: {name}")

    size = resource.get("size")
    if not isinstance(size, int) or size < 1:
        raise ValueError(f"Invalid generated resource size for {name}: {size}")

    resource_dir = downloads_dir / use_case
    sub_folder = resource.get("sub_folder")
    if sub_folder is not None:
        sub_folder_path = Path(sub_folder)
        if sub_folder_path.is_absolute() or ".." in sub_folder_path.parts:
            raise ValueError(
                f"Invalid generated resource sub_folder for {name}: {sub_folder}"
            )
        resource_dir = resource_dir / sub_folder_path

    output = resource_dir / name
    if output.is_file() and output.stat().st_size == size:
        logging.info("File %s exists, skipping generation.", output)
        return

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(bytes(size))
    logging.info("- Generated %s.", output)


def _set_up_generated_resources(args: argparse.Namespace) -> None:
    """
    Generate project-local resources declared alongside model downloads.

    :param args:    Parsed command line arguments.
    """
    with open(args.use_case_resources_file, encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)

    use_case_filter = set(args.use_case)
    downloads_dir = args.downloads_dir.resolve()
    for use_case in manifest:
        use_case_name = use_case["name"]
        if use_case_filter and use_case_name not in use_case_filter:
            continue

        for resource in use_case.get("generated_resources", []):
            _write_generated_resource(downloads_dir, use_case_name, resource)


def _configure_logging(args: argparse.Namespace) -> None:
    """
    Configure MLEK logging after dependency-light argument handling.

    :param args:    Parsed command line arguments.
    """
    from mlek_tools.setup.logging_config import (
        LoggingOptions,
        configure_logging,
    )

    configure_logging(
        DEFAULT_LOG_FILE,
        LoggingOptions(
            console_level=args.log_level,
            file_mode="a",
            show_thread=args.parallel > 1,
        ),
    )


def main():
    """Command line entry point."""
    args = _parse_args()
    _configure_logging(args)
    _validate_args(args)
    venv_path = _set_up_resources(args)
    _set_up_generated_resources(args)
    logging.info("Model resources are ready. Tool environment: %s", venv_path)


if __name__ == "__main__":
    main()
