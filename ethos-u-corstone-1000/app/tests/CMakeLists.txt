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

include(util_functions)

set(TEST_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/outputs")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

if (TARGET_PLATFORM STREQUAL "native")
    find_package(Catch2 3 QUIET)
    if (NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.0.tar.gz
            URL_HASH SHA256=9650c55e497759cc39b977e45524bc8acb15256061c112080916ab6cb0b1ea66
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(Catch2)
    endif()
elseif (CORSTONE1000_FVP_TESTS_ENABLED)
    if (NOT PYTHON)
        message(FATAL_ERROR "PYTHON is not set. Call find_python() before adding tests.")
    endif()

    set(_CORSTONE1000_FVP_CTEST_RUNNER
        "${CMAKE_SOURCE_DIR}/scripts/py/ctest_run_fvp.py")
    if (NOT CORSTONE1000_FVP_CTEST_RUNNER OR
            NOT EXISTS "${CORSTONE1000_FVP_CTEST_RUNNER}")
        set(CORSTONE1000_FVP_CTEST_RUNNER
            "${_CORSTONE1000_FVP_CTEST_RUNNER}"
            CACHE FILEPATH
            "Host-side script used by CTest to run one Corstone-1000 FVP test."
            FORCE)
    endif()
endif()

# Render and register one target-side FVP shell script for a CTest.
#
# The rendered script runs inside the packaged MMC image. It contains the
# target-side FVP_COMMAND declared by the test, emits stable APP_* markers
# consumed by the Python FVP runner, and checks any declared FVP_OUTPUTS paths.
# The generated script path is appended to the DIRECT_DRIVE_FVP_TEST_SCRIPTS
# global property so corstone1000_mmc.cmake packages it into the configured
# tests directory.
#
# Arguments:
#   output_variable
#     Name of the variable that receives the target-side script file name.
#   test_name
#     CTest name used in generated status markers and the script file name.
#   outputs_variable
#     Name of the variable containing target-side output paths that must exist
#     after the command runs.
#   ARGN
#     Target-side command list to render into the script.
#
# Outputs:
#   DIRECT_DRIVE_FVP_TEST_SCRIPTS
#     Global property receives the generated host-side script path.
#
# Examples:
#   test_name=direct-drive-inference-runner-zero-input
#   outputs_variable=target_outputs containing <output-dir>/result.bin
#   ARGN=<bin-dir>/direct-drive-inference-runner --output <output-dir>/result.bin
#     -> renders fvp/direct-drive-inference-runner-zero-input.sh
#     -> appends that script path to DIRECT_DRIVE_FVP_TEST_SCRIPTS
#     -> output_variable receives direct-drive-inference-runner-zero-input.sh
function(_direct_drive_generate_fvp_test_script output_variable test_name outputs_variable)
    make_safe_file_name(test_name_safe "${test_name}")
    set(script_dir "${CMAKE_CURRENT_BINARY_DIR}/fvp")
    set(script_path "${script_dir}/${test_name_safe}.sh")
    file(MAKE_DIRECTORY "${script_dir}")

    set(render_command
        "${PYTHON}"
        "${CMAKE_SOURCE_DIR}/scripts/py/ctest_render_target_script.py"
        "--templates-dir" "${CMAKE_SOURCE_DIR}/scripts/py/fvp_ctest/templates"
        "--test-name" "${test_name}"
        "--output-dir" "./${CORSTONE1000_MMC_OUTPUT_DIR}"
        "--output-script" "${script_path}")
    foreach(output_file IN LISTS ${outputs_variable})
        list(APPEND render_command "--expected-output" "${output_file}")
    endforeach()
    list(APPEND render_command "--command" ${ARGN})
    execute_process(
        COMMAND ${render_command}
        COMMAND_ERROR_IS_FATAL ANY)

    set_property(GLOBAL APPEND PROPERTY
        DIRECT_DRIVE_FVP_TEST_SCRIPTS
        "${script_path}")
    set("${output_variable}" "${test_name_safe}.sh" PARENT_SCOPE)
endfunction()

# Add a CTest entry for a direct-drive command line executable.
#
# This helper lets use-case tests describe one CLI invocation once and have it
# registered appropriately for the active target platform. Native tests run the
# command directly on the host. Corstone-1000 FVP tests use FVP_COMMAND as the
# exact target-side command to render into a shell script that is packaged into
# the MMC image. This avoids inferring MMC paths from host path extensions.
#
# Options:
#   WILL_FAIL
#     Mark the CTest as expected to fail.
#
# One-value arguments:
#   NAME
#     CTest name to register.
#   PASS_REGULAR_EXPRESSION
#     Optional CTest PASS_REGULAR_EXPRESSION property.
#   FAIL_REGULAR_EXPRESSION
#     Optional CTest FAIL_REGULAR_EXPRESSION property.
#
# Multi-value arguments:
#   COMMAND
#     Native command line to run on the host.
#   FVP_COMMAND
#     Target-side command line to render for Corstone-1000 FVP execution. This
#     must use paths as they appear inside the MMC image. Prefer the
#     CORSTONE1000_MMC_*_DIR layout variables when constructing those paths.
#   FVP_OUTPUTS
#     Target-side output paths that the generated FVP test script must find
#     after FVP_COMMAND exits successfully.
#   LABELS
#     CTest labels to attach. FVP tests also receive corstone-1000 and fvp.
#   REQUIRED_FILES
#     Extra files required by native tests.
#
# Examples:
#   COMMAND direct-drive-inference-runner --model /tmp/model.tflite
#   FVP_COMMAND <bin-dir>/direct-drive-inference-runner
#       --model <model-dir>/model_vela_Z2048.tflite
function(direct_drive_add_cli_test)
    # Parse the common CLI-test shape used by both native and FVP-backed tests.
    set(options WILL_FAIL)
    set(one_value_args
        FAIL_REGULAR_EXPRESSION
        NAME
        PASS_REGULAR_EXPRESSION)
    set(multi_value_args
        COMMAND
        FVP_COMMAND
        FVP_OUTPUTS
        LABELS
        REQUIRED_FILES)
    cmake_parse_arguments(TEST
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if (NOT TEST_NAME)
        message(FATAL_ERROR "direct_drive_add_cli_test requires NAME.")
    endif()
    if (NOT TEST_COMMAND)
        message(FATAL_ERROR "direct_drive_add_cli_test requires COMMAND.")
    endif()
    if (CORSTONE1000_FVP_TESTS_ENABLED AND
            NOT TEST_FVP_COMMAND)
        message(FATAL_ERROR
            "direct_drive_add_cli_test requires FVP_COMMAND for "
            "Corstone-1000 FVP tests: ${TEST_NAME}.")
    endif()

    # Register the test either as a direct host command or as an FVP invocation.
    if (TARGET_PLATFORM STREQUAL "native")
        add_test(NAME "${TEST_NAME}" COMMAND ${TEST_COMMAND})
        set(test_required_files "${TEST_REQUIRED_FILES}")
        set(test_labels "${TEST_LABELS}")
    elseif (CORSTONE1000_FVP_TESTS_ENABLED)
        _direct_drive_generate_fvp_test_script(
            target_test_script
            "${TEST_NAME}"
            TEST_FVP_OUTPUTS
            ${TEST_FVP_COMMAND})
        add_test(
            NAME "${TEST_NAME}"
            COMMAND "${PYTHON}"
                "${CORSTONE1000_FVP_CTEST_RUNNER}"
                "${CMAKE_SOURCE_DIR}"
                "${CMAKE_BINARY_DIR}"
                "${TEST_NAME}"
                "${CORSTONE1000_MMC_IMAGE}"
                "${CORSTONE1000_MMC_TEST_DIR}/${target_test_script}")
        set(test_required_files
            "${CORSTONE1000_MMC_IMAGE}"
            "${CORSTONE1000_FVP_CTEST_RUNNER}"
            "${CMAKE_CURRENT_BINARY_DIR}/fvp/${target_test_script}")
        set(test_labels "${TEST_LABELS}" "corstone-1000" "fvp")
        set(test_skip_return_code 77)
    else()
        return()
    endif()

    # Apply the optional CTest properties supplied by individual test cases.
    set_tests_properties("${TEST_NAME}" PROPERTIES
        LABELS "${test_labels}")

    if (test_required_files)
        set_tests_properties("${TEST_NAME}" PROPERTIES
            REQUIRED_FILES "${test_required_files}")
    endif()
    if (test_skip_return_code)
        set_tests_properties("${TEST_NAME}" PROPERTIES
            SKIP_RETURN_CODE "${test_skip_return_code}")
    endif()
    if (TEST_PASS_REGULAR_EXPRESSION)
        set_tests_properties("${TEST_NAME}" PROPERTIES
            PASS_REGULAR_EXPRESSION "${TEST_PASS_REGULAR_EXPRESSION}")
    endif()
    if (TEST_FAIL_REGULAR_EXPRESSION)
        set_tests_properties("${TEST_NAME}" PROPERTIES
            FAIL_REGULAR_EXPRESSION "${TEST_FAIL_REGULAR_EXPRESSION}")
    endif()
    if (TEST_WILL_FAIL)
        set_tests_properties("${TEST_NAME}" PROPERTIES
            WILL_FAIL TRUE)
    endif()
endfunction()

foreach(use_case IN LISTS ENABLED_USE_CASES)
    add_subdirectory("use_case/${use_case}")
endforeach()
