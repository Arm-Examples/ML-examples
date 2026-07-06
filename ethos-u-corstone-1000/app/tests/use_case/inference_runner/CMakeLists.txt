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

get_property(APP_TARGET GLOBAL PROPERTY
    "DIRECT_DRIVE_USE_CASE_TARGET_inference_runner")
if (NOT APP_TARGET)
    message(FATAL_ERROR "No direct-drive target registered for inference_runner.")
endif()
set(MODEL_NAME "mobilenet_v2_1.0_224_INT8.tflite")
set(ZERO_INPUT_NAME "zero_224x224x3_u8.bin")
set(OUTPUT_NAME "inference_runner_zero_output.bin")
set(RANDOM_OUTPUT_NAME "inference_runner_random_output.bin")
cmake_path(GET MODEL_NAME STEM LAST_ONLY MODEL_STEM)
set(VELA_MODEL_NAME "${MODEL_STEM}_vela_Z2048.tflite")

# Verify that the generic inference runner exposes binary input support and
# does not advertise image-classification-specific options.
direct_drive_add_cli_test(
    NAME direct-drive-inference-runner-help
    COMMAND "${APP_TARGET}" --help
    FVP_COMMAND "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}" --help
    LABELS "direct-drive;inference-runner;unit"
    PASS_REGULAR_EXPRESSION "--input-bin"
    FAIL_REGULAR_EXPRESSION "--input-image|--labels")

# Verify that the generic inference runner rejects image-specific input.
direct_drive_add_cli_test(
    NAME direct-drive-inference-runner-rejects-image-input
    COMMAND "${APP_TARGET}" --input-image unsupported.bmp
    FVP_COMMAND "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}" --input-image unsupported.bmp
    LABELS "direct-drive;inference-runner;unit"
    WILL_FAIL TRUE)

# Run MobileNet V2 with an explicit zero-filled binary input and write the
# inference output.
set(OUTPUT "${TEST_OUTPUT_DIR}/${OUTPUT_NAME}")
direct_drive_add_cli_test(
    NAME direct-drive-inference-runner-zero-input
    COMMAND
        "${APP_TARGET}"
        --model "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}"
        --input-bin "${DOWNLOADS_DIR_ABS}/img_class/test-inputs/${ZERO_INPUT_NAME}"
        --output "${OUTPUT}"
        --ref
    FVP_COMMAND
        "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}"
        --model "./${CORSTONE1000_MMC_MODEL_DIR}/${VELA_MODEL_NAME}"
        --lib "./${CORSTONE1000_MMC_LIB_DIR}/libethosu_op_delegate.so"
        --input-bin "./${CORSTONE1000_MMC_INPUT_DIR}/${ZERO_INPUT_NAME}"
        --output "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
        --ref
    FVP_OUTPUTS "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
    LABELS "direct-drive;inference-runner;integration;requires-model-resources"
    REQUIRED_FILES
        "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}"
        "${DOWNLOADS_DIR_ABS}/img_class/test-inputs/${ZERO_INPUT_NAME}")

# Run MobileNet V2 without an input file so the inference runner exercises its
# generated-input path and writes the inference output.
set(RANDOM_OUTPUT "${TEST_OUTPUT_DIR}/${RANDOM_OUTPUT_NAME}")
direct_drive_add_cli_test(
    NAME direct-drive-inference-runner-random-input
    COMMAND
        "${APP_TARGET}"
        --model "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}"
        --output "${RANDOM_OUTPUT}"
        --ref
    FVP_COMMAND
        "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}"
        --model "./${CORSTONE1000_MMC_MODEL_DIR}/${VELA_MODEL_NAME}"
        --lib "./${CORSTONE1000_MMC_LIB_DIR}/libethosu_op_delegate.so"
        --output "./${CORSTONE1000_MMC_OUTPUT_DIR}/${RANDOM_OUTPUT_NAME}"
        --ref
    FVP_OUTPUTS "./${CORSTONE1000_MMC_OUTPUT_DIR}/${RANDOM_OUTPUT_NAME}"
    LABELS "direct-drive;inference-runner;integration;requires-model-resources"
    REQUIRED_FILES "${DOWNLOADS_DIR_ABS}/img_class/${MODEL_NAME}")
