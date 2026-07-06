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
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR "aarch64")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Concrete toolchain file for the Bootlin AArch64 Linux toolchain.
# The toolchain must already be installed on the host and available on PATH.
#
# We intentionally do not bake an installation root into this file. The support
# model is that developers manage the Bootlin toolchain themselves, and this
# toolchain file only describes how CMake should use it once its binaries are
# discoverable.
set(BOOTLIN_TRIPLET "aarch64-linux" CACHE STRING
    "Prefix for the Bootlin AArch64 toolchain binaries")

# Resolve every cross tool via PATH rather than by constructing absolute paths.
# This keeps the file portable across developer machines and also avoids
# coupling the build to a specific unpack location.
#
# Bootlin's compiler frontends are symlinks to a shared wrapper binary. The
# wrapper behavior depends on the invoked tool name, so we validate that the
# tools exist on PATH but keep the configured compiler/archive tool values as
# the triplet-prefixed names rather than resolved absolute paths.
#
# Arguments:
#   tool_suffix
#     Tool suffix appended to BOOTLIN_TRIPLET, for example gcc, g++, or strip.
function(require_bootlin_tool tool_suffix)
    string(MAKE_C_IDENTIFIER "BOOTLIN_${tool_suffix}_PATH" tool_var)

    find_program(${tool_var}
        NAMES "${BOOTLIN_TRIPLET}-${tool_suffix}"
        NO_CACHE)

    if (NOT ${tool_var})
        message(FATAL_ERROR
            "Could not find ${BOOTLIN_TRIPLET}-${tool_suffix} on PATH. "
            "Install the Bootlin AArch64 Linux toolchain and add its bin directory to PATH.")
    endif()
endfunction()

require_bootlin_tool(gcc)
require_bootlin_tool(g++)
require_bootlin_tool(as)
require_bootlin_tool(gcc-ar)
require_bootlin_tool(gcc-ranlib)
require_bootlin_tool(nm)
require_bootlin_tool(objcopy)
require_bootlin_tool(objdump)
require_bootlin_tool(readelf)
require_bootlin_tool(strip)

set(CMAKE_C_COMPILER   "${BOOTLIN_TRIPLET}-gcc")
set(CMAKE_CXX_COMPILER "${BOOTLIN_TRIPLET}-g++")
set(CMAKE_ASM_COMPILER "${BOOTLIN_TRIPLET}-gcc")

# Let GCC drive the actual linker selection. For this Bootlin wrapper setup,
# forcing CMAKE_LINKER caused CMake to generate invalid "gcc qc ..." archive
# rules for static libraries.
set(CMAKE_AR                 "${BOOTLIN_TRIPLET}-gcc-ar")
set(CMAKE_C_COMPILER_AR      "${BOOTLIN_TRIPLET}-gcc-ar")
set(CMAKE_CXX_COMPILER_AR    "${BOOTLIN_TRIPLET}-gcc-ar")
set(CMAKE_RANLIB             "${BOOTLIN_TRIPLET}-gcc-ranlib")
set(CMAKE_C_COMPILER_RANLIB  "${BOOTLIN_TRIPLET}-gcc-ranlib")
set(CMAKE_CXX_COMPILER_RANLIB "${BOOTLIN_TRIPLET}-gcc-ranlib")
set(CMAKE_NM                 "${BOOTLIN_TRIPLET}-nm")
set(CMAKE_OBJCOPY            "${BOOTLIN_TRIPLET}-objcopy")
set(CMAKE_OBJDUMP            "${BOOTLIN_TRIPLET}-objdump")
set(CMAKE_READELF            "${BOOTLIN_TRIPLET}-readelf")
set(CMAKE_STRIP              "${BOOTLIN_TRIPLET}-strip")

# Ask the compiler for its active sysroot instead of inferring one from an
# install directory. This allows relocated toolchains to keep working as long as
# the compiler wrapper itself is functional and on PATH.
execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
    OUTPUT_VARIABLE BOOTLIN_MACHINE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE BOOTLIN_MACHINE_RESULT
)

if (NOT BOOTLIN_MACHINE_RESULT EQUAL 0 OR NOT BOOTLIN_MACHINE MATCHES "musl")
    message(FATAL_ERROR
        "${CMAKE_C_COMPILER} does not appear to target musl "
        "(reported target: ${BOOTLIN_MACHINE}). "
        "Ensure the Bootlin AArch64 musl toolchain is first on PATH.")
endif()

execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-sysroot
    OUTPUT_VARIABLE BOOTLIN_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE BOOTLIN_SYSROOT_RESULT
)

if (NOT BOOTLIN_SYSROOT_RESULT EQUAL 0 OR BOOTLIN_SYSROOT STREQUAL "" OR NOT EXISTS "${BOOTLIN_SYSROOT}")
    message(FATAL_ERROR
        "Failed to resolve a valid sysroot from ${CMAKE_C_COMPILER} -print-sysroot. "
        "Ensure the Bootlin toolchain is installed correctly and on PATH.")
endif()

set(CMAKE_SYSROOT "${BOOTLIN_SYSROOT}")

# Restrict target library/include/package discovery to the target sysroot while
# still allowing host-side build tools to be found on the developer machine.
set(CMAKE_FIND_ROOT_PATH "${BOOTLIN_SYSROOT}")
# Enable automatic lib64 lookup for target-side library discovery.
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)

# Host tools discovered via find_program() must stay on the build machine.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_compile_definitions(
    _GNU_SOURCE
    _FORTIFY_SOURCE=2
    FLATBUFFERS_LOCALE_INDEPENDENT=0
)

add_compile_options(
    -march=armv9.2-a
    -mbranch-protection=standard
    -fstack-protector-strong
    -O2
    -Wformat
    -Wformat-security
    -Werror=format-security
    -fcanon-prefix-map
)

add_link_options(
    -march=armv9.2-a
    -mbranch-protection=standard
    -fstack-protector-strong
    -O2
    -D_FORTIFY_SOURCE=2
    -Wformat
    -Wformat-security
    -Werror=format-security
    -Wl,-z,relro,-z,now
)
