#----------------------------------------------------------------------------
#  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
#  affiliates <open-source-office@arm.com>
#  SPDX-License-Identifier: Apache-2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#----------------------------------------------------------------------------
include_guard(GLOBAL)

# Find the Python virtual environment used by project helper scripts.
#
# The helper prefers the resource setup environment under DOWNLOADS_DIR/env, or
# an explicit PYTHON_VENV cache value when supplied. The virtual environment is
# created and populated by setup_model_resources.sh before CMake configure.
#
# Inputs:
#   DOWNLOADS_DIR
#     Resource directory produced by setup_model_resources.sh.
#   PYTHON_VENV
#     Optional cache path to an existing virtual environment.
#
# Outputs:
#   PYTHON
#     Parent-scope path to the virtual environment Python interpreter.
#   PYTHON_VENV
#     Parent-scope path to the selected virtual environment directory.
#
function(find_python)
    if (PYTHON_VENV)
        set(python_venv "${PYTHON_VENV}")
    else()
        set(python_venv "${DOWNLOADS_DIR_ABS}/env")
    endif()
    cmake_path(ABSOLUTE_PATH python_venv
        BASE_DIRECTORY "${CMAKE_SOURCE_DIR}"
        NORMALIZE
        OUTPUT_VARIABLE python_venv)

    if (CMAKE_HOST_WIN32)
        set(python "${python_venv}/Scripts/python.exe")
    else()
        set(python "${python_venv}/bin/python3")
    endif()

    if (NOT EXISTS "${python}")
        message(FATAL_ERROR
            "Python virtual environment not found: ${python_venv}. "
            "Run ${CMAKE_SOURCE_DIR}/setup_model_resources.sh first, "
            "or pass -DPYTHON_VENV=<path-to-existing-venv>.")
    endif()

    message(STATUS "Using Python virtual environment: ${python_venv}")
    message(STATUS "Using Python interpreter: ${python}")

    set(PYTHON_VENV "${python_venv}" CACHE PATH
        "Python virtual environment used for Corstone-1000 project helper scripts."
        FORCE)
    set(PYTHON "${python}" PARENT_SCOPE)
    set(PYTHON_VENV "${python_venv}" PARENT_SCOPE)
endfunction()
