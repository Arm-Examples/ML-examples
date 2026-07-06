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

set(FIXTURES_DIR "${CMAKE_CURRENT_LIST_DIR}/fixtures")
get_property(APP_TARGET GLOBAL PROPERTY
    "DIRECT_DRIVE_USE_CASE_TARGET_image_classification")
if (NOT APP_TARGET)
    message(FATAL_ERROR "No direct-drive target registered for image_classification.")
endif()
set(MODEL_NAME "mobilenet_v2_1.0_224_INT8.tflite")
set(ZERO_INPUT_NAME "zero_224x224x3_u8.bin")
set(LABELS_NAME "mobilenet_labels.txt")
set(OUTPUT_NAME "image_classification_zero_output.bin")
cmake_path(GET MODEL_NAME STEM LAST_ONLY MODEL_STEM)
set(VELA_MODEL_NAME "${MODEL_STEM}_vela_Z2048.tflite")

if (TARGET_PLATFORM STREQUAL "native")
    add_executable(direct-drive-image-classification-tests
        ImageClassificationAppTests.cpp)

    target_link_libraries(direct-drive-image-classification-tests PRIVATE
        Catch2::Catch2
        direct_drive_image_classification_app)
endif()

# Verify that the image classification CLI exposes image and label options.
direct_drive_add_cli_test(
    NAME direct-drive-image-classification-help
    COMMAND "${APP_TARGET}" --help
    FVP_COMMAND "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}" --help
    LABELS "direct-drive;image-classification;unit"
    PASS_REGULAR_EXPRESSION "--input-image;--labels")

if (TARGET_PLATFORM STREQUAL "native")
    # Verify label-file parsing and lookup behaviour in the native app layer.
    add_test(
        NAME    direct-drive-image-classification-labels
        COMMAND direct-drive-image-classification-tests
            --test-data-dir "${FIXTURES_DIR}")

    set_tests_properties(direct-drive-image-classification-labels PROPERTIES
        LABELS "direct-drive;image-classification;unit")
endif()

# Run MobileNet V2 classification with labels and an explicit zero-filled
# binary input, then write the inference output.
set(OUTPUT "${TEST_OUTPUT_DIR}/${OUTPUT_NAME}")
direct_drive_add_cli_test(
    NAME direct-drive-image-classification-zero-input
    COMMAND
        "${APP_TARGET}"
        --model "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}"
        --labels "${FIXTURES_DIR}/${LABELS_NAME}"
        --input-bin "${DOWNLOADS_DIR_ABS}/img_class/test-inputs/${ZERO_INPUT_NAME}"
        --output "${OUTPUT}"
        --ref
    FVP_COMMAND
        "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}"
        --model "./${CORSTONE1000_MMC_MODEL_DIR}/${VELA_MODEL_NAME}"
        --lib "./${CORSTONE1000_MMC_LIB_DIR}/libethosu_op_delegate.so"
        --labels "./${CORSTONE1000_MMC_LABEL_DIR}/${LABELS_NAME}"
        --input-bin "./${CORSTONE1000_MMC_INPUT_DIR}/${ZERO_INPUT_NAME}"
        --output "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
        --ref
    FVP_OUTPUTS "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
    LABELS "direct-drive;image-classification;integration;requires-model-resources"
    REQUIRED_FILES
        "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}"
        "${FIXTURES_DIR}/${LABELS_NAME}"
        "${DOWNLOADS_DIR_ABS}/img_class/test-inputs/${ZERO_INPUT_NAME}")
