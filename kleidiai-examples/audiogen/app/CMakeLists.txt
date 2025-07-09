#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.16)

project(audiogen_runner)

include(ExternalProject)

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_TOOLCHAIN_FILE)
  list(APPEND TOOLCHAIN_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if(ANDROID_ABI)
  list(APPEND TOOLCHAIN_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI})
endif()

if(NOT TF_SRC_PATH)
  include(FetchContent)

  message(STATUS "Downloading TensorFlow")

  if(ENABLE_SME2_EXPERIMENTAL)
    message(STATUS "Enabling SME2 experimental")
    FetchContent_Declare(
      tensorflow_src
      GIT_REPOSITORY https://github.com/tensorflow/tensorflow.git
      GIT_TAG 56edbb30b2a41798bb87d6cd4d9bb6cadd3b134b
    )

    FetchContent_MakeAvailable(tensorflow_src)

    set(SME2_FLAG ON)
    set(TENSORFLOW_SOURCE_DIR ${tensorflow_src_SOURCE_DIR})
    set(TENSORFLOW_LITE_SOURCE_DIR "${tensorflow_src_SOURCE_DIR}/tensorflow/lite")

    FetchContent_GetProperties(tensorflow_src)
    if(NOT tensorflow_src_POPULATED)
      message(FATAL_ERROR "TensorFlow was not populated")
    endif()

    # Patch DownloadPThreadPool.cmake
    file(READ "${TENSORFLOW_LITE_SOURCE_DIR}/cmake/DownloadPThreadPool.cmake" pthreadpool_content)
    string(REPLACE
      "https://github.com/google/pthreadpool/archive/dcc9f28589066af0dbd4555579281230abbf74dd.zip"
      "https://github.com/google/pthreadpool/archive/c2ba5c50bb58d1397b693740cf75fad836a0d1bf.zip"
      pthreadpool_content "${pthreadpool_content}")
    string(REPLACE
      "SHA256=b193fcb18abf0d5bdb989968ccd8e274ae0ce8d89b0f1a61763582fb716b259c"
      "SHA256=516ba8d05c30e016d7fd7af6a7fc74308273883f857faf92bc9bb630ab6dba2c"
      pthreadpool_content "${pthreadpool_content}")
    file(WRITE "${TENSORFLOW_LITE_SOURCE_DIR}/cmake/DownloadPThreadPool.cmake" "${pthreadpool_content}")

    # Patch xnnpack.cmake
    file(READ "${TENSORFLOW_LITE_SOURCE_DIR}/tools/cmake/modules/xnnpack.cmake" xnnpack_content)
    string(REPLACE
      "GIT_REPOSITORY https://github.com/google/XNNPACK"
      "GIT_REPOSITORY https://github.com/gmiodice/XNNPACK.git"
      xnnpack_content "${xnnpack_content}")
    string(REPLACE
      "GIT_TAG 8b3d5687eb8e4a266731ad8a288de6c813c2959b"
      "GIT_TAG 4ee40b90b941ce6e525c486d6cba44fa50bcffc6"
      xnnpack_content "${xnnpack_content}")
    file(WRITE "${TENSORFLOW_LITE_SOURCE_DIR}/tools/cmake/modules/xnnpack.cmake" "${xnnpack_content}")

    message(STATUS "Applied SME2 experimental patches to TensorFlow Lite")
  else()
      FetchContent_Declare(
      tensorflow_src
      GIT_REPOSITORY https://github.com/tensorflow/tensorflow.git
      GIT_TAG 84dd28bbc29d75e6a6d917eb2998e4e8ea90ec56
    )

    FetchContent_MakeAvailable(tensorflow_src)

    set(SME2_FLAG OFF)
    set(TENSORFLOW_SOURCE_DIR ${tensorflow_src_SOURCE_DIR})
  endif()

else()
  set(TENSORFLOW_SOURCE_DIR ${TF_SRC_PATH})
endif()

set(FLATBUFFERS_SOURCE_DIR ${TENSORFLOW_SOURCE_DIR}/tensorflow/lite/tools/cmake/native_tools/flatbuffers)
set(FLATBUFFERS_BIN_DIR ${CMAKE_BINARY_DIR}/flatbuffers-host-bin)
set(SENTENCEPIECE_SOURCE_DIR ${CMAKE_BINARY_DIR}/sentencepiece)

# Step 1: Build flatc ---
ExternalProject_Add(
  flatc_build
  PREFIX flatc
  DOWNLOAD_COMMAND ""
  SOURCE_DIR ${FLATBUFFERS_SOURCE_DIR}
  BINARY_DIR ${FLATBUFFERS_BIN_DIR}
  INSTALL_COMMAND ""
)

# Step 2: Build SentencePiece ---
ExternalProject_Add(
  sentencepiece_src
  GIT_REPOSITORY https://github.com/google/sentencepiece.git
  GIT_TAG        v0.2.0
  SOURCE_DIR ${SENTENCEPIECE_SOURCE_DIR}
  CMAKE_ARGS
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    ${TOOLCHAIN_CMAKE_ARGS}
  INSTALL_COMMAND ""
)

#Get the path to the sentencepiece lib
ExternalProject_Get_Property(sentencepiece_src BINARY_DIR)
set(SENTENCEPIECE_LIB ${BINARY_DIR}/src/libsentencepiece.a)

## Step 4: Build the audiogen app ---
# Define source
set(SRCS audiogen.cpp)

add_executable(audiogen ${SRCS})

set(XNNPACK_ENABLE_ARM_SME2 ${SME2_FLAG} CACHE BOOL "" FORCE)
set(TFLITE_HOST_TOOLS_DIR "${FLATBUFFERS_BIN_DIR}/_deps/flatbuffers-build" CACHE PATH "Host tools directory")

# Because flatc is not available at configure time,
# this workaround places a placeholder (an empty file called flatc) at the expected install location.
file(WRITE ${FLATBUFFERS_BIN_DIR}/_deps/flatbuffers-build/flatc "")

# LiteRT CMake configure stage
add_subdirectory(
  "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
  "${CMAKE_CURRENT_BINARY_DIR}/tensorflow-lite"
  EXCLUDE_FROM_ALL
)

# Delete the placeholder flatc file after the LiteRT CMake configuration stage is complete.
file(REMOVE ${FLATBUFFERS_BIN_DIR}/_deps/flatbuffers-build/flatc "")

add_dependencies(tensorflow-lite flatc_build)

target_compile_definitions(tensorflow-lite PRIVATE
  TF_MAJOR_VERSION=0
  TF_MINOR_VERSION=0
  TF_PATCH_VERSION=0
  TF_VERSION_SUFFIX=""
)

# Include headers
target_include_directories(audiogen PRIVATE
  ${TENSORFLOW_SOURCE_DIR}/tensorflow/lite
  ${SENTENCEPIECE_SOURCE_DIR}/src
)

# Link with dependencies
target_link_libraries(audiogen
  tensorflow-lite
  ${SENTENCEPIECE_LIB}
)

# Ensure dependency build order
add_dependencies(audiogen flatc_build sentencepiece_src)
