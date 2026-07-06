/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or
 * its affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TFLITE_BACKEND_OPTIONS_HPP
#define TFLITE_BACKEND_OPTIONS_HPP

#include <string>
#include <vector>

namespace arm::app::fwk::tflite {

/**
 * @brief TensorFlow Lite backend configuration.
 */
struct TfliteBackendOptions {
    /** Default Ethos-U device node used by the external delegate. */
    static constexpr char kDefaultDeviceName[] = "/dev/ethosu0";
    /** Default delegate timeout value in nanoseconds. */
    static constexpr char kDefaultTimeout[] = "60000000000";

    /** Path to the TensorFlow Lite model file. */
    std::string modelPath{};
    /** Path to the TensorFlow Lite external delegate library. */
    std::string delegateLibPath{};
    /** Ethos-U device node passed to the external delegate. */
    std::string deviceName{kDefaultDeviceName};
    /** External delegate timeout value in nanoseconds. */
    std::string timeout{kDefaultTimeout};
    /** Ethos-U PMU event identifiers indexed by counter. */
    std::vector<int> pmuEvents{};
    /** Enable the Ethos-U cycle counter through the delegate. */
    bool enableCycleCounter{false};
    /** Use TensorFlow Lite reference kernels instead of optimized kernels. */
    bool useRefKernels{false};
};

} // namespace arm::app::fwk::tflite

#endif // TFLITE_BACKEND_OPTIONS_HPP
