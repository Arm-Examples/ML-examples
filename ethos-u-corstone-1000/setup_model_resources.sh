#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or
# its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python_script="${script_dir}/scripts/py/setup_model_resources.py"
requirements_file="${script_dir}/scripts/py/requirements.txt"
downloads_dir="${script_dir}/downloads"
venv_dir=""
python_bin="${PYTHON:-python3}"
args=()
show_help=false

while (($# > 0)); do
    case "$1" in
        -h|--help)
            show_help=true
            args+=("$1")
            shift
            ;;
        --downloads-dir)
            if (($# < 2)); then
                echo "error: --downloads-dir requires a value" >&2
                exit 2
            fi
            downloads_dir="$2"
            args+=("$1" "$2")
            shift 2
            ;;
        --downloads-dir=*)
            downloads_dir="${1#*=}"
            args+=("$1")
            shift
            ;;
        --venv-dir)
            if (($# < 2)); then
                echo "error: --venv-dir requires a value" >&2
                exit 2
            fi
            venv_dir="$2"
            args+=("$1" "$2")
            shift 2
            ;;
        --venv-dir=*)
            venv_dir="${1#*=}"
            args+=("$1")
            shift
            ;;
        *)
            args+=("$1")
            shift
            ;;
    esac
done

if [[ "${show_help}" == true ]]; then
    exec "${python_bin}" "${python_script}" "${args[@]}"
fi

if [[ -z "${venv_dir}" ]]; then
    venv_dir="${downloads_dir%/}/env"
    args+=("--venv-dir" "${venv_dir}")
fi

if [[ ! -x "${venv_dir}/bin/python3" ]]; then
    "${python_bin}" -m venv "${venv_dir}"
fi

"${venv_dir}/bin/python3" -m pip install --no-cache-dir -r "${requirements_file}"
exec "${venv_dir}/bin/python3" "${python_script}" "${args[@]}"
