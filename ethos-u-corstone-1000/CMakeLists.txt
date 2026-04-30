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
cmake_minimum_required(VERSION 3.21.0)

# Some TensorFlow Lite third-party dependencies still use old CMake policy
# versions. CMake 4 rejects those unless this compatibility floor is set.
if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
        "Policy compatibility for legacy third-party CMake projects")
endif()

# Set language standards.
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# Make the standard a requirement => prevent fallback to previous
# supported standard
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# We want to pass standard C/C++ flags, without gnu extensions
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/configuration_options")

include(user_options)
include(corstone1000_mmc_layout)
include(python_env)
include(use_cases)

# Resolve the Bootlin toolchain file for Corstone-1000 AArch64 builds when requested.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND
        TARGET_PLATFORM STREQUAL "corstone-1000-aarch64")
    set(CMAKE_TOOLCHAIN_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/toolchains/aarch64-bootlin-linux-musl.cmake)
endif()

project(arm_ethos_u_corstone_1000
        VERSION         26.06.0
        DESCRIPTION     "Arm Ethos-U direct-drive examples for Corstone-1000"
        LANGUAGES       C CXX ASM)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

cmake_path(ABSOLUTE_PATH DOWNLOADS_DIR
    BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
    NORMALIZE
    OUTPUT_VARIABLE DOWNLOADS_DIR_ABS)

find_python()

include(mlek)
include(tensorflow_lite)

if (ETHOS_U_NPU_ENABLED AND ETHOS_U_NPU_BUILD_DELEGATE)
    include(ethos_u_linux_driver_stack)
endif()

discover_use_cases("${CMAKE_CURRENT_SOURCE_DIR}/app/src/use_case")
add_subdirectory(app/src)

enable_testing()
add_subdirectory(app/tests)

if (CORSTONE1000_PACKAGE_MMC)
    include(corstone1000_mmc)
    corstone1000_add_mmc_package_target()
endif()
