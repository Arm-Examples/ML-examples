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

set(TENSORFLOW_GIT_REPOSITORY
    "https://github.com/tensorflow/tensorflow.git"
    CACHE STRING "TensorFlow Git repository")
set(TENSORFLOW_GIT_TAG
    "v2.20.0"
    CACHE STRING "Pinned TensorFlow Git revision")

if (TENSORFLOW_LITE_DIR)
    if (NOT EXISTS "${TENSORFLOW_LITE_DIR}/tensorflow/lite/CMakeLists.txt")
        message(FATAL_ERROR
            "Invalid TENSORFLOW_LITE_DIR: "
            "${TENSORFLOW_LITE_DIR}")
    endif()

    FetchContent_Declare(tensorflow
        SOURCE_DIR    "${TENSORFLOW_LITE_DIR}"
        SOURCE_SUBDIR "tensorflow/lite")
else()
    FetchContent_Declare(tensorflow
        GIT_REPOSITORY "${TENSORFLOW_GIT_REPOSITORY}"
        GIT_TAG        "${TENSORFLOW_GIT_TAG}"
        SOURCE_SUBDIR  "tensorflow/lite")
endif()

# TensorFlow Lite 2.20 still uses FetchContent_Populate() internally for
# several dependencies. The third-party implementation needs the old policy
# default until TensorFlow updates its CMake modules.
if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
    set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
endif()

# Resolve and populate the TensorFlow source tree managed by FetchContent.
#
# Arguments:
#   output_var
#     Name of the variable that receives the TensorFlow source directory.
function(_tflite_get_tensorflow_source_dir output_var)
    FetchContent_GetProperties(tensorflow)
    if (NOT tensorflow_POPULATED)
        message(STATUS "Fetching TensorFlow sources")
        FetchContent_Populate(tensorflow)
    endif()

    set(${output_var} "${tensorflow_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# Build the host flatc tool from TensorFlow's native-tools project.
#
# Cross builds need a host flatc binary to generate TensorFlow Lite sources.
# This helper configures and builds that host tool with host compilers, then
# returns the directory containing the generated flatc executable.
#
# Arguments:
#   tensorflow_source_dir
#     Populated TensorFlow source directory.
#   output_var
#     Name of the variable that receives the host tools directory.
function(_tflite_build_host_flatc tensorflow_source_dir output_var)
    set(_tflite_host_flatc_source_dir
        "${tensorflow_source_dir}/tensorflow/lite/tools/cmake/native_tools/flatbuffers")
    set(_tflite_host_flatc_build_dir
        "${PROJECT_BINARY_DIR}/tensorflow-lite-host-tools/flatbuffers")
    set(_tflite_host_flatc_dir
        "${_tflite_host_flatc_build_dir}/_deps/flatbuffers-build")
    set(_tflite_host_flatc
        "${_tflite_host_flatc_dir}/flatc")

    if (NOT EXISTS "${_tflite_host_flatc_source_dir}/CMakeLists.txt")
        message(FATAL_ERROR
            "TensorFlow Lite host flatc CMake project not found: "
            "${_tflite_host_flatc_source_dir}")
    endif()

    find_program(_tflite_host_c_compiler
        NAMES cc gcc clang
        REQUIRED
        NO_CMAKE_FIND_ROOT_PATH)
    find_program(_tflite_host_cxx_compiler
        NAMES c++ g++ clang++
        REQUIRED
        NO_CMAKE_FIND_ROOT_PATH)

    if (NOT EXISTS "${_tflite_host_flatc}")
        message(STATUS "Configuring host flatc from TensorFlow sources")

        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    -S "${_tflite_host_flatc_source_dir}"
                    -B "${_tflite_host_flatc_build_dir}"
                    -DCMAKE_BUILD_TYPE=Release
                    -DCMAKE_POLICY_DEFAULT_CMP0169=OLD
                    "-DCMAKE_C_COMPILER=${_tflite_host_c_compiler}"
                    "-DCMAKE_CXX_COMPILER=${_tflite_host_cxx_compiler}"
            RESULT_VARIABLE _tflite_host_flatc_configure_result)

        if (NOT _tflite_host_flatc_configure_result EQUAL 0)
            message(FATAL_ERROR "Failed to configure host flatc build")
        endif()

        message(STATUS "Building flatc...")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    --build "${_tflite_host_flatc_build_dir}"
                    --target flatc
                    --parallel
            RESULT_VARIABLE _tflite_host_flatc_build_result
            OUTPUT_VARIABLE _tflite_host_flatc_build_output
            ERROR_VARIABLE _tflite_host_flatc_build_error)

        if (NOT _tflite_host_flatc_build_result EQUAL 0)
            message(STATUS "${_tflite_host_flatc_build_output}")
            message(STATUS "${_tflite_host_flatc_build_error}")
            message(FATAL_ERROR "Failed to build host flatc")
        endif()
    endif()

    if (NOT EXISTS "${_tflite_host_flatc}")
        message(FATAL_ERROR "Host flatc was not created: ${_tflite_host_flatc}")
    endif()

    set(${output_var} "${_tflite_host_flatc_dir}" PARENT_SCOPE)
endfunction()

# Add TensorFlow Lite's CMake project from the populated TensorFlow sources.
#
# This keeps TensorFlow Lite excluded from the default all target until local
# targets link against it.
function(_tflite_add_tensorflow_lite)
    _tflite_get_tensorflow_source_dir(_tflite_tensorflow_source_dir)
    FetchContent_GetProperties(tensorflow)

    add_subdirectory("${_tflite_tensorflow_source_dir}/tensorflow/lite"
        "${tensorflow_BINARY_DIR}" EXCLUDE_FROM_ALL)
endfunction()

if (CMAKE_CROSSCOMPILING)
    set(TFLITE_ENABLE_NNAPI OFF CACHE BOOL "Disable TensorFlow Lite NNAPI" FORCE)
    set(CCACHE_BINARY "CCACHE_BINARY-NOTFOUND" CACHE FILEPATH
        "Disable third-party ccache auto-detection" FORCE)

    if (TFLITE_HOST_TOOLS_DIR)
        set(_tflite_host_tools_default "${TFLITE_HOST_TOOLS_DIR}")
    else()
        _tflite_get_tensorflow_source_dir(_tflite_tensorflow_source_dir)
        _tflite_build_host_flatc("${_tflite_tensorflow_source_dir}"
            _tflite_host_tools_default)
    endif()

    set(TFLITE_HOST_TOOLS_DIR "${_tflite_host_tools_default}" CACHE PATH
        "Directory containing host TensorFlow Lite build tools" FORCE)

    if (NOT EXISTS "${TFLITE_HOST_TOOLS_DIR}/flatc")
        message("TFLITE_HOST_TOOLS_DIR: ${TFLITE_HOST_TOOLS_DIR}")
        message(FATAL_ERROR
            "TFLITE_HOST_TOOLS_DIR must point to a directory containing host flatc. "
            "Pass -DTFLITE_HOST_TOOLS_DIR=<path-to-directory-containing-flatc> "
            "or allow this project to build flatc from the TensorFlow sources.")
    endif()
endif()

if (ETHOS_U_NPU_ENABLED AND ETHOS_U_NPU_BUILD_DELEGATE)
    set(BUILD_DELEGATE ON)
    # Match the Ethos-U driver stack thirdparty wrapper. TensorFlow Lite's
    # dependency graph is not warning-clean with the cross flags.
    add_compile_options(-w)
endif()

_tflite_add_tensorflow_lite()

if (NOT TARGET tensorflow-lite)
    message(FATAL_ERROR "tensorflow-lite target not found")
endif()

target_compile_definitions(tensorflow-lite PRIVATE
    TF_MAJOR_VERSION=2
    TF_MINOR_VERSION=20
    TF_PATCH_VERSION=0
    TF_VERSION_SUFFIX="")
