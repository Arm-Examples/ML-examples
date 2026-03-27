#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.16)

project(audiogen_runner)

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(LITERT_DISABLE_KLEIDIAI OFF CACHE BOOL "" FORCE)
set(XNNPACK_ENABLE_ARM_SME ON CACHE BOOL "" FORCE)
set(XNNPACK_ENABLE_ARM_SME2 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  litert
  GIT_REPOSITORY https://github.com/google-ai-edge/LiteRT.git
  GIT_TAG e0883b0d1b50a924b41d09b03107c6e0e1260910
)
FetchContent_Populate(litert)

set(_litert_cmake "${litert_SOURCE_DIR}/litert/CMakeLists.txt")
file(READ "${_litert_cmake}" _litert_txt)
string(REPLACE
  "set(_tflite_extra_args -DCMAKE_POLICY_VERSION_MINIMUM=3.5)"
  "set(_tflite_extra_args -DCMAKE_POLICY_VERSION_MINIMUM=3.5)\n"
  "if(DEFINED XNNPACK_ENABLE_ARM_SME)\n"
  "  list(APPEND _tflite_extra_args -DXNNPACK_ENABLE_ARM_SME=${XNNPACK_ENABLE_ARM_SME})\n"
  "endif()\n"
  "if(DEFINED XNNPACK_ENABLE_ARM_SME2)\n"
  "  list(APPEND _tflite_extra_args -DXNNPACK_ENABLE_ARM_SME2=${XNNPACK_ENABLE_ARM_SME2})\n"
  "endif()"
  _litert_txt
  "${_litert_txt}"
)
file(WRITE "${_litert_cmake}" "${_litert_txt}")

add_subdirectory("${litert_SOURCE_DIR}/litert" "${litert_BINARY_DIR}")

# LiteRT uses absl::log::DieIfNull but doesn't link absl::die_if_null explicitly.
if(TARGET absl::die_if_null)
  target_link_libraries(litert_cc_api PUBLIC absl::die_if_null)
endif()

FetchContent_Declare(
  sentencepiece
  GIT_REPOSITORY https://github.com/google/sentencepiece.git
  GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(sentencepiece)

## Build the audiogen app ---
# Define source
set(SRCS audiogen.cpp)

add_executable(audiogen ${SRCS})

# Include headers
target_include_directories(audiogen PRIVATE
  ${CMAKE_BINARY_DIR}/include
  ${litert_BINARY_DIR}/include
  ${sentencepiece_SOURCE_DIR}/src
)

# Link with dependencies
target_link_libraries(audiogen
  litert_runtime
  absl::log
  absl::log_internal_check_op
  sentencepiece-static
)
