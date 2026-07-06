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
set(TFLITE_FWK_TARGET ml_tflite_framework_impl)

add_library(${TFLITE_FWK_TARGET} STATIC
    TfliteModel.cpp
    TfliteTensor.cpp)

target_include_directories(${TFLITE_FWK_TARGET} PUBLIC
    "${PROJECT_SOURCE_DIR}/app/src")

target_link_libraries(${TFLITE_FWK_TARGET} PUBLIC
    mlek::log
    mlek::ml_framework_iface
    tensorflow-lite)

if (ETHOS_U_NPU_ENABLED)
    target_compile_definitions(${TFLITE_FWK_TARGET} PUBLIC
        ETHOS_U_NPU_ENABLED)
endif()
