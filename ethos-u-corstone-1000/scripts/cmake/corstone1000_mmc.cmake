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
include(corstone1000_mmc_layout)

# Add the Corstone-1000 MMC packaging target.
#
# The stage-mmc target stages selected use-case binaries and the Ethos-U
# delegate under mmc-staging and strips Debug staged copies. The package-mmc
# target depends on stage-mmc and runs prep_mmc.py with the staged runtime
# files, requested image path, runtime resources, and generated FVP CTest
# scripts. The staging target depends on every packaged build output so the
# staged files are refreshed after relevant binaries or generated scripts are
# rebuilt.
#
# Inputs:
#   CORSTONE1000_MMC_IMAGE
#     Cache file path for the generated MMC image.
#   CMAKE_BUILD_TYPE
#     Selects whether staged runtime files are stripped.
#   ENABLED_USE_CASES
#     Controls which use-case binaries and runtime resources are packaged.
#
# Requires:
#   ethosu_op_delegate
#     Target that produces the Ethos-U delegate library.
function(corstone1000_add_mmc_package_target)
    if (NOT PYTHON)
        message(FATAL_ERROR "PYTHON is not set. Call find_python() before packaging MMC images.")
    endif()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_STRIP)
        message(FATAL_ERROR "CMAKE_STRIP is not set. Cannot strip Debug MMC staging files.")
    endif()

    # Establish the fixed parts of the prep_mmc.py invocation. The selected
    # use-case binaries and resource files are appended later after target
    # metadata has been collected.
    set(CORSTONE1000_MMC_IMAGE_SIZE "64MiB")
    set(mmc_staging_dir "${CMAKE_BINARY_DIR}/mmc-staging")
    set(mmc_staging_bin_dir "${mmc_staging_dir}/bin")
    set(mmc_staging_lib_dir "${mmc_staging_dir}/lib")
    set(mmc_command
        "${PYTHON}"
        "${CMAKE_SOURCE_DIR}/scripts/py/prep_mmc.py"
        "--layout" "${CORSTONE1000_MMC_LAYOUT_FILE}"
        "--image" "${CORSTONE1000_MMC_IMAGE}"
        "--image-size" "${CORSTONE1000_MMC_IMAGE_SIZE}")
    set(mmc_stage_commands
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${mmc_staging_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${mmc_staging_bin_dir}" "${mmc_staging_lib_dir}")
    set(mmc_dependencies ethosu_op_delegate)

    if (NOT TARGET ethosu_op_delegate)
        message(FATAL_ERROR
            "CORSTONE1000_PACKAGE_MMC requires the Ethos-U delegate target. "
            "Reconfigure with ETHOS_U_NPU_BUILD_DELEGATE=ON or CORSTONE1000_PACKAGE_MMC=OFF.")
    endif()
    set(staged_delegate_library
        "${mmc_staging_lib_dir}/$<TARGET_FILE_NAME:ethosu_op_delegate>")
    list(APPEND mmc_stage_commands
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:ethosu_op_delegate>" "${staged_delegate_library}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND mmc_stage_commands
            COMMAND "${CMAKE_STRIP}" --strip-debug "${staged_delegate_library}")
    endif()
    list(APPEND mmc_command "--library" "${staged_delegate_library}")

    # Aggregate the role-specific resource lists registered by each selected
    # use-case target.
    set(mmc_models "")
    set(mmc_inputs "")
    set(mmc_labels "")

    # Resolve each enabled use case to its executable target, validate that the
    # target metadata is self-consistent, and collect the package resources that
    # were registered on that target by direct_drive_set_use_case_metadata().
    foreach(use_case IN LISTS ENABLED_USE_CASES)
        get_property(use_case_target GLOBAL PROPERTY
            "DIRECT_DRIVE_USE_CASE_TARGET_${use_case}")

        if (NOT use_case_target)
            message(FATAL_ERROR
                "No direct-drive target metadata registered for use case: "
                "${use_case}")
        endif()
        if (NOT TARGET "${use_case_target}")
            message(FATAL_ERROR
                "Registered direct-drive target does not exist: "
                "${use_case_target}")
        endif()

        get_target_property(registered_use_case
            "${use_case_target}"
            DIRECT_DRIVE_USE_CASE)
        if (NOT registered_use_case STREQUAL use_case)
            message(FATAL_ERROR
                "Registered target ${use_case_target} belongs to use case "
                "${registered_use_case}, not ${use_case}.")
        endif()

        set(staged_use_case_binary
            "${mmc_staging_bin_dir}/$<TARGET_FILE_NAME:${use_case_target}>")
        list(APPEND mmc_stage_commands
            COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${use_case_target}>" "${staged_use_case_binary}")
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND mmc_stage_commands
                COMMAND "${CMAKE_STRIP}" --strip-debug "${staged_use_case_binary}")
        endif()

        list(APPEND mmc_command "--binary" "${staged_use_case_binary}")
        list(APPEND mmc_dependencies "${use_case_target}")

        get_target_property(use_case_models "${use_case_target}" DIRECT_DRIVE_MODELS)
        get_target_property(use_case_inputs "${use_case_target}" DIRECT_DRIVE_INPUTS)
        get_target_property(use_case_labels "${use_case_target}" DIRECT_DRIVE_LABELS)

        if (use_case_models)
            list(APPEND mmc_models ${use_case_models})
        endif()
        if (use_case_inputs)
            list(APPEND mmc_inputs ${use_case_inputs})
        endif()
        if (use_case_labels)
            list(APPEND mmc_labels ${use_case_labels})
        endif()
    endforeach()

    # Convert collected resource lists into repeated prep_mmc.py role options.
    # The Python helper accepts either exact paths or globs for these values and
    # resolves them at package time.
    list(REMOVE_DUPLICATES mmc_models)
    foreach(mmc_model IN LISTS mmc_models)
        list(APPEND mmc_command "--model" "${mmc_model}")
    endforeach()

    list(REMOVE_DUPLICATES mmc_inputs)
    foreach(mmc_input IN LISTS mmc_inputs)
        list(APPEND mmc_command "--input" "${mmc_input}")
    endforeach()

    list(REMOVE_DUPLICATES mmc_labels)
    foreach(mmc_label IN LISTS mmc_labels)
        list(APPEND mmc_command "--label" "${mmc_label}")
    endforeach()

    get_property(mmc_test_scripts GLOBAL PROPERTY DIRECT_DRIVE_FVP_TEST_SCRIPTS)
    if (mmc_test_scripts)
        list(REMOVE_DUPLICATES mmc_test_scripts)
        foreach(mmc_test_script IN LISTS mmc_test_scripts)
            list(APPEND mmc_command "--test" "${mmc_test_script}")
            list(APPEND mmc_dependencies "${mmc_test_script}")
        endforeach()
    endif()

    # Depend on the build products that CMake knows about. Resource globs are
    # intentionally expanded by prep_mmc.py at package time rather than by
    # CMake, so they are not represented as explicit target dependencies.
    list(REMOVE_DUPLICATES mmc_dependencies)

    add_custom_target(stage-mmc
        ${mmc_stage_commands}
        DEPENDS ${mmc_dependencies}
        COMMENT "Staging Corstone-1000 MMC runtime files: ${mmc_staging_dir}"
        VERBATIM)

    add_custom_target(package-mmc ALL
        COMMAND ${mmc_command}
        DEPENDS stage-mmc
        COMMENT "Packaging Corstone-1000 MMC image: ${CORSTONE1000_MMC_IMAGE}"
        VERBATIM)
endfunction()
