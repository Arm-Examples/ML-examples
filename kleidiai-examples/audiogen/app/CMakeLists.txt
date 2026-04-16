#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20)

project(audiogen_runner LANGUAGES C CXX)

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  litert
  GIT_REPOSITORY https://github.com/google-ai-edge/LiteRT.git
  GIT_TAG        c3eb2c5884c099206487d717e1782ba512ef2871
)

FetchContent_GetProperties(litert)
if(NOT litert_POPULATED)
  FetchContent_Populate(litert)
endif()

# Disable GPU support before adding the LiteRT subdirectory.
set(LITERT_ENABLE_GPU OFF CACHE BOOL "Disable LiteRT GPU support")

set(LITERT_SOURCE_DIR "${litert_SOURCE_DIR}")
set(LITERT_BUILD_DIR  "${CMAKE_CURRENT_BINARY_DIR}/litert_build")

add_subdirectory(
  "${LITERT_SOURCE_DIR}/litert"
  "${LITERT_BUILD_DIR}"
  EXCLUDE_FROM_ALL
)

FetchContent_Declare(
  sentencepiece
  GIT_REPOSITORY https://github.com/google/sentencepiece.git
  GIT_TAG        v0.2.0
)

FetchContent_MakeAvailable(sentencepiece)

add_executable(audiogen
  audiogen.cpp
)

target_include_directories(audiogen
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${sentencepiece_SOURCE_DIR}/src
)

set(LITERT_LIBS
  litert_c_api
  litert_cc_api
  litert_cc_internal
  litert_cc_options
  litert_compiler_plugin
  litert_core
  litert_core_model
  litert_runtime
)

set(LINK_GROUP_START "")
set(LINK_GROUP_END "")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
  set(LINK_GROUP_START "-Wl,--start-group")
  set(LINK_GROUP_END   "-Wl,--end-group")
endif()

target_link_libraries(audiogen
  PRIVATE
    ${LINK_GROUP_START}

    absl::die_if_null
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_internal_check_op
    absl::log_internal_message
    absl::string_view
    absl::span
    absl::status
    absl::statusor

    ${LITERT_LIBS}
    tensorflow-lite
    sentencepiece-static

    ${LINK_GROUP_END}
)
