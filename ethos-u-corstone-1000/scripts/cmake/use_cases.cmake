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

# Discover available use-case source directories and select the requested set.
#
# The discovered directory names become the valid USE_CASE_BUILD cache choices.
# USE_CASE_BUILD=all enables every discovered use case; otherwise
# USE_CASE_BUILD is treated as a CMake list of explicit use-case names.
#
# Arguments:
#   use_cases_dir
#     Directory containing one subdirectory per use case.
#
# Outputs:
#   ENABLED_USE_CASES
#     Parent-scope list of use cases selected for this configure.
function(discover_use_cases use_cases_dir)
    if (NOT IS_DIRECTORY "${use_cases_dir}")
        message(FATAL_ERROR "Use-case source directory not found: ${use_cases_dir}")
    endif()

    # Discover use-case directories so new use cases are available without
    # maintaining a separate hard-coded list.
    file(GLOB discovered_use_case_paths
        RELATIVE "${use_cases_dir}"
        CONFIGURE_DEPENDS
        "${use_cases_dir}/*")
    set(AVAILABLE_USE_CASES "")
    foreach(use_case IN LISTS discovered_use_case_paths)
        if (IS_DIRECTORY "${use_cases_dir}/${use_case}")
            list(APPEND AVAILABLE_USE_CASES "${use_case}")
        endif()
    endforeach()
    list(SORT AVAILABLE_USE_CASES)
    set_property(CACHE USE_CASE_BUILD PROPERTY STRINGS
        all
        ${AVAILABLE_USE_CASES})

    # Interpret USE_CASE_BUILD=all as every discovered use case. Otherwise,
    # treat USE_CASE_BUILD as a CMake list of explicitly requested use cases.
    if ("${USE_CASE_BUILD}" STREQUAL "all")
        set(enabled_use_cases ${AVAILABLE_USE_CASES})
    else()
        set(enabled_use_cases ${USE_CASE_BUILD})
    endif()

    if (NOT enabled_use_cases)
        message(FATAL_ERROR
            "USE_CASE_BUILD must be 'all' or one or more supported use-cases.")
    endif()

    # Fail during configure if the user requests a use case that does not have
    # a matching source directory.
    foreach(use_case IN LISTS enabled_use_cases)
        list(FIND AVAILABLE_USE_CASES "${use_case}" use_case_index)
        if (use_case_index EQUAL -1)
            message(FATAL_ERROR
                "Unsupported USE_CASE_BUILD entry '${use_case}'. "
                "Discovered use-cases are: ${AVAILABLE_USE_CASES}.")
        endif()
    endforeach()

    message(STATUS "Building use-cases: ${enabled_use_cases}")
    set(ENABLED_USE_CASES ${enabled_use_cases} PARENT_SCOPE)
endfunction()

# Register direct-drive use-case metadata on an executable target.
#
# The metadata is consumed by CTest definitions and by Corstone-1000 MMC
# packaging. Keeping it on the target lets each use case define its own runtime
# resources beside the executable target, while downstream code can still
# aggregate resources without hard-coding individual use-case names.
#
# One-value arguments:
#   USE_CASE
#     Use-case name as it appears in ENABLED_USE_CASES.
#   TARGET
#     Executable target for the use case.
#
# Multi-value arguments:
#   MODELS
#     Model files or glob patterns to package into the configured model
#     directory.
#   INPUTS
#     Input files or glob patterns to package into the configured input
#     directory.
#   LABELS
#     Label files or glob patterns to package into the configured labels
#     directory.
function(direct_drive_set_use_case_metadata)
    set(one_value_args TARGET USE_CASE)
    set(multi_value_args INPUTS LABELS MODELS)
    cmake_parse_arguments(METADATA
        ""
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if (NOT METADATA_USE_CASE)
        message(FATAL_ERROR "direct_drive_set_use_case_metadata requires USE_CASE.")
    endif()
    if (NOT METADATA_TARGET)
        message(FATAL_ERROR "direct_drive_set_use_case_metadata requires TARGET.")
    endif()
    if (NOT TARGET "${METADATA_TARGET}")
        message(FATAL_ERROR
            "direct_drive_set_use_case_metadata target does not exist: "
            "${METADATA_TARGET}")
    endif()

    set_target_properties("${METADATA_TARGET}" PROPERTIES
        DIRECT_DRIVE_INPUTS "${METADATA_INPUTS}"
        DIRECT_DRIVE_LABELS "${METADATA_LABELS}"
        DIRECT_DRIVE_MODELS "${METADATA_MODELS}"
        DIRECT_DRIVE_USE_CASE "${METADATA_USE_CASE}")
    set_property(GLOBAL PROPERTY
        "DIRECT_DRIVE_USE_CASE_TARGET_${METADATA_USE_CASE}"
        "${METADATA_TARGET}")
endfunction()
