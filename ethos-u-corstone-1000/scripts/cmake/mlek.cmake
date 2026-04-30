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

include(FetchContent)

set(MLEK_GIT_REPOSITORY
    "https://gitlab.arm.com/artificial-intelligence/ethos-u/ml-embedded-evaluation-kit.git"
    CACHE STRING "MLEK Git repository")
set(MLEK_GIT_TAG
    "dd4c1b0096333ddd35e7dc63f1efd045f4e00d50"
    CACHE STRING "Pinned MLEK Git revision")

# MLEK override: Set the provider to "External" for MLEK to only provide the
# framework interface. We don't need MLEK provisioned runtimes (ExecuTorch
# and TensorFlow Lite Micro) in this project.
set(MLEK_RUNTIME_PROVIDER "External")

message(STATUS "Adding MLEK project targets")

if (MLEK_SOURCE_DIR)
    set(MLEK_LIB_DIR "${MLEK_SOURCE_DIR}/source/lib")
    if (NOT EXISTS "${MLEK_LIB_DIR}")
        message(FATAL_ERROR "Invalid MLEK_SOURCE_DIR: ${MLEK_SOURCE_DIR}")
    endif()
    add_subdirectory("${MLEK_LIB_DIR}" "${CMAKE_BINARY_DIR}/mlek" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(mlek
        GIT_REPOSITORY "${MLEK_GIT_REPOSITORY}"
        GIT_TAG        "${MLEK_GIT_TAG}"
        GIT_SUBMODULES
            "dependencies/cmsis-dsp"
            "dependencies/cmsis-6"
        SOURCE_SUBDIR  "source/lib"
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(mlek)
endif()

message(STATUS "MLEK lib targets included")
