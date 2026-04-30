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

if (NOT EXISTS "${CORSTONE1000_MMC_LAYOUT_FILE}")
    message(FATAL_ERROR
        "Corstone-1000 MMC layout file not found: "
        "${CORSTONE1000_MMC_LAYOUT_FILE}")
endif()

set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS
    "${CORSTONE1000_MMC_LAYOUT_FILE}")

file(READ "${CORSTONE1000_MMC_LAYOUT_FILE}" _CORSTONE1000_MMC_LAYOUT_JSON)
string(JSON CORSTONE1000_MMC_IMAGE_NAME
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" image_name)
set(CORSTONE1000_MMC_IMAGE
    "${CMAKE_BINARY_DIR}/${CORSTONE1000_MMC_IMAGE_NAME}"
    CACHE FILEPATH "Packaged Corstone-1000 MMC image")
string(JSON CORSTONE1000_MMC_BIN_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories bin)
string(JSON CORSTONE1000_MMC_LIB_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories lib)
string(JSON CORSTONE1000_MMC_MODEL_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories models)
string(JSON CORSTONE1000_MMC_INPUT_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories inputs)
string(JSON CORSTONE1000_MMC_LABEL_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories labels)
string(JSON CORSTONE1000_MMC_OUTPUT_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories outputs)
string(JSON CORSTONE1000_MMC_TEST_DIR
    GET "${_CORSTONE1000_MMC_LAYOUT_JSON}" directories tests)
