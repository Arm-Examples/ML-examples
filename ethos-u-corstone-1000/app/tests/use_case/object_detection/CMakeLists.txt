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
    "DIRECT_DRIVE_USE_CASE_TARGET_object_detection")
if (NOT APP_TARGET)
    message(FATAL_ERROR "No direct-drive target registered for object_detection.")
endif()

set(IMAGE_NAME "couple.bmp")
set(MODEL_NAME "yolo-fastest_192_face_v4.tflite")
set(OUTPUT_NAME "object_detection_couple_output.bin")
cmake_path(GET MODEL_NAME STEM LAST_ONLY MODEL_STEM)
set(VELA_MODEL_NAME "${MODEL_STEM}_vela_Z2048.tflite")

# Verify that the object detection CLI exposes image input support and does not
# advertise image-classification label options.
direct_drive_add_cli_test(
    NAME direct-drive-object-detection-help
    COMMAND "${APP_TARGET}" --help
    FVP_COMMAND "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}" --help
    LABELS "direct-drive;object-detection;unit"
    PASS_REGULAR_EXPRESSION "--input-image"
    FAIL_REGULAR_EXPRESSION "--labels")

set(OUTPUT "${TEST_OUTPUT_DIR}/${OUTPUT_NAME}")
# Run the YOLO Fastest face model against the sample image and write the
# detection output.
direct_drive_add_cli_test(
    NAME direct-drive-object-detection-couple
    COMMAND
        "${APP_TARGET}"
        --model "${DOWNLOADS_DIR_ABS}/object_detection/${MODEL_NAME}"
        --input-ppm "${DOWNLOADS_DIR_ABS}/object_detection/samples/${IMAGE_NAME}"
        --output "${OUTPUT}"
        --ref
    FVP_COMMAND
        "./${CORSTONE1000_MMC_BIN_DIR}/${APP_TARGET}"
        --model "./${CORSTONE1000_MMC_MODEL_DIR}/${VELA_MODEL_NAME}"
        --lib "./${CORSTONE1000_MMC_LIB_DIR}/libethosu_op_delegate.so"
        --input-ppm "./${CORSTONE1000_MMC_INPUT_DIR}/${IMAGE_NAME}"
        --output "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
        --ref
    FVP_OUTPUTS "./${CORSTONE1000_MMC_OUTPUT_DIR}/${OUTPUT_NAME}"
    LABELS "direct-drive;object-detection;integration;requires-model-resources"
    REQUIRED_FILES
        "${DOWNLOADS_DIR_ABS}/object_detection/${MODEL_NAME}"
        "${DOWNLOADS_DIR_ABS}/object_detection/samples/${IMAGE_NAME}")
