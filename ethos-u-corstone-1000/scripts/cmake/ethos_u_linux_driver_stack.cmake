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

set(ETHOS_U_NPU_LINUX_STACK_GIT_REPOSITORY
    "https://gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-linux-driver-stack.git"
    CACHE STRING "Ethos-U Linux driver stack Git repository")
set(ETHOS_U_NPU_LINUX_STACK_GIT_TAG
    "26.02-rc2"
    CACHE STRING "Pinned Ethos-U Linux driver stack Git revision")
if (ETHOS_U_NPU_LINUX_STACK_DIR)
    if (NOT EXISTS "${ETHOS_U_NPU_LINUX_STACK_DIR}/driver_library/CMakeLists.txt" OR
        NOT EXISTS "${ETHOS_U_NPU_LINUX_STACK_DIR}/delegate/CMakeLists.txt")
        message(FATAL_ERROR
            "Invalid ETHOS_U_NPU_LINUX_STACK_DIR: "
            "${ETHOS_U_NPU_LINUX_STACK_DIR}")
    endif()

    set(ethos_u_linux_driver_stack_SOURCE_DIR
        "${ETHOS_U_NPU_LINUX_STACK_DIR}")
else()
    FetchContent_Declare(ethos_u_linux_driver_stack
        GIT_REPOSITORY "${ETHOS_U_NPU_LINUX_STACK_GIT_REPOSITORY}"
        GIT_TAG        "${ETHOS_U_NPU_LINUX_STACK_GIT_TAG}"
        SOURCE_SUBDIR  "_unused")
    FetchContent_MakeAvailable(ethos_u_linux_driver_stack)
endif()

set(ETHOS_U_NPU_LINUX_STACK_DIR
    "${ethos_u_linux_driver_stack_SOURCE_DIR}"
    CACHE PATH "Resolved Ethos-U Linux driver stack source directory" FORCE)

add_subdirectory("${ETHOS_U_NPU_LINUX_STACK_DIR}/driver_library"
    "${CMAKE_BINARY_DIR}/driver_library" EXCLUDE_FROM_ALL)

add_subdirectory("${ETHOS_U_NPU_LINUX_STACK_DIR}/delegate"
    "${CMAKE_BINARY_DIR}/delegate" EXCLUDE_FROM_ALL)

target_include_directories(ethosu PUBLIC
    "${ETHOS_U_NPU_LINUX_STACK_DIR}/kernel/include")
