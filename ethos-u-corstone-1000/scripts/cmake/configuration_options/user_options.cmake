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

include(util_functions)

set(VALID_TARGET_PLATFORMS
    native
    corstone-1000-aarch64)

USER_OPTION(TARGET_PLATFORM
    "Target platform to build for: native or corstone-1000-aarch64."
    native
    STRING)

set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS ${VALID_TARGET_PLATFORMS})
list(FIND VALID_TARGET_PLATFORMS "${TARGET_PLATFORM}" TARGET_PLATFORM_INDEX)
if (TARGET_PLATFORM_INDEX EQUAL -1)
    string(JOIN ", " VALID_TARGET_PLATFORMS_TEXT ${VALID_TARGET_PLATFORMS})
    message(FATAL_ERROR
        "Invalid TARGET_PLATFORM '${TARGET_PLATFORM}'. Expected one of: "
        "${VALID_TARGET_PLATFORMS_TEXT}.")
endif()

USER_OPTION(MLEK_SOURCE_DIR
    "Existing MLEK source directory; when empty FetchContent is used."
    ""
    PATH)

USER_OPTION(TENSORFLOW_LITE_DIR
    "Existing TensorFlow source directory; when empty FetchContent is used."
    ""
    PATH)

USER_OPTION(DOWNLOADS_DIR
    "Directory containing model resources produced by setup_model_resources.py --downloads-dir."
    "${CMAKE_SOURCE_DIR}/downloads"
    PATH)

USER_OPTION(PYTHON_VENV
    "Python virtual environment used for Corstone-1000 project helper scripts."
    ""
    PATH)

USER_OPTION(USE_CASE_BUILD
    "Use-cases to build, or all."
    all
    STRING)

USER_OPTION(CORSTONE1000_MMC_LAYOUT_FILE
    "JSON file defining the Corstone-1000 MMC image layout."
    "${CMAKE_SOURCE_DIR}/resources/mmc_layout.json"
    FILEPATH)

if (TARGET_PLATFORM STREQUAL "corstone-1000-aarch64")
    USER_OPTION(CORSTONE1000_PACKAGE_MMC
        "Package the Corstone-1000 MMC image as part of the default build."
        ON
        BOOL)
    USER_OPTION(ETHOS_U_NPU_ENABLED
        "If Arm Ethos-U NPU is enabled in the target system."
        ON
        BOOL)
    USER_OPTION(ETHOS_U_NPU_BUILD_DELEGATE
        "Build libethosu_op_delegate.so."
        ON
        BOOL)
    USER_OPTION(ETHOS_U_NPU_LINUX_STACK_DIR
        "Existing Ethos-U Linux driver stack source directory; when empty FetchContent is used."
        ""
        PATH)
else()
    USER_OPTION(CORSTONE1000_PACKAGE_MMC
        "Package the Corstone-1000 MMC image as part of the default build."
        OFF
        BOOL)
endif()

set(CORSTONE1000_FVP_TESTS_DEFAULT OFF)
if (TARGET_PLATFORM STREQUAL "corstone-1000-aarch64" AND
        CORSTONE1000_PACKAGE_MMC)
    set(CORSTONE1000_FVP_TESTS_DEFAULT ON)
endif()

USER_OPTION(CORSTONE1000_FVP_TESTS_ENABLED
    "Register Corstone-1000 FVP CTests."
    "${CORSTONE1000_FVP_TESTS_DEFAULT}"
    BOOL)

if (CORSTONE1000_FVP_TESTS_ENABLED AND
        NOT TARGET_PLATFORM STREQUAL "corstone-1000-aarch64")
    message(FATAL_ERROR
        "CORSTONE1000_FVP_TESTS_ENABLED requires "
        "TARGET_PLATFORM=corstone-1000-aarch64.")
endif()

if (CORSTONE1000_FVP_TESTS_ENABLED AND
        NOT CORSTONE1000_PACKAGE_MMC)
    message(FATAL_ERROR
        "CORSTONE1000_FVP_TESTS_ENABLED requires "
        "CORSTONE1000_PACKAGE_MMC=ON.")
endif()
