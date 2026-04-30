/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 Arm Limited and/or
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

/**
 * Inspired by: https://gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-linux-driver-stack/-/blob/26.02/utils/inference_runner/inference_runner.cpp?ref_type=tags
 */

#ifndef RUNNER_OPTIONS_HPP
#define RUNNER_OPTIONS_HPP

#include "fwk/tflite/TfliteBackendOptions.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace arm::app::common {

/**
 * @brief Input file interpretation mode selected from the command line.
 */
enum class InputMode {
    /** No input mode has been selected. */
    kUnset,
    /** Inputs are raw binary tensor data. */
    kBinary,
    /** Inputs are image files decoded into the model input tensor. */
    kImage,
};

/**
 * @brief Command-line option identifiers supported by use case applications.
 */
enum class CliOptionId {
    kHelp,
    kModel,
    kLabels,
    kInputBin,
    kInputImage,
    kOutput,
    kRef,
    kProfiling,
#if defined(ETHOS_U_NPU_ENABLED)
    kCycles,
    kDevice,
    kTimeout,
    kDelegateLib,
    kPmu,
#endif
};

/**
 * @brief Command-line option set supported by a use case application.
 */
struct CliOptionsConfig {
    /** Enabled command-line options. */
    std::vector<CliOptionId> optionIds{};
    /** Allow the app to run without an input file. */
    bool allowMissingInput{false};
};

/**
 * @brief Runtime options shared by command-line use cases.
 */
struct RunnerOptions {
#if defined(ETHOS_U_NPU_ENABLED)
    /** Maximum number of Ethos-U PMU counters exposed by the delegate. */
    static constexpr size_t kMaxNumCounters = 8;
#else
    /** PMU counters are unavailable when Ethos-U support is disabled. */
    static constexpr size_t kMaxNumCounters = 0;
#endif

    /** Default Ethos-U device node used by the external delegate. */
    static constexpr const char* kDefaultDevice =
        fwk::tflite::TfliteBackendOptions::kDefaultDeviceName;
    /** Default delegate timeout value in nanoseconds. */
    static constexpr const char* kDefaultTimeout =
        fwk::tflite::TfliteBackendOptions::kDefaultTimeout;

    /** Path to the TensorFlow Lite model file. */
    std::string modelPath{};
    /** Optional labels file path used by classification-style use cases. */
    std::string labelsPath{};
    /** Ethos-U device node passed to the external delegate. */
    std::string deviceName{kDefaultDevice};
    /** Path to the TensorFlow Lite external delegate library. */
    std::string delegateLibPath{};
    /** External delegate timeout value in nanoseconds. */
    std::string timeout{kDefaultTimeout};
    /** Selected input interpretation mode. */
    InputMode inputMode{InputMode::kUnset};
    /** Input file paths processed by the selected use case. */
    std::vector<std::string> inputs{};
    /** Optional output file paths matched by input index. */
    std::vector<std::string> outputs{};
    /** Ethos-U PMU event identifiers indexed by counter. */
    std::array<int, kMaxNumCounters> pmuEvents{};
    /** Enable the Ethos-U cycle counter through the delegate. */
    bool enableCycleCounter{false};
    /** Run only the first input and print a timing report. */
    bool profilingMode{false};
    /** Use TensorFlow Lite reference kernels instead of optimized kernels. */
    bool useRefKernels{false};
    /** Use generated random input data when no input file is provided. */
    bool useRandomInputIfMissing{false};
    /** Print help and exit before validation or use-case execution. */
    bool helpRequested{false};

    /**
     * @brief Add an input path and establish the input mode.
     *
     * The first input sets the mode. Later inputs must use the same mode.
     *
     * @param  filePath  Input file path to append.
     * @param  mode      Interpretation mode for the input file.
     * @return           True when the input is accepted, otherwise false.
     */
    bool AddInput(const std::string& filePath, InputMode mode);

    /**
     * @brief Validate option consistency and referenced file paths.
     *
     * @return  True when all required options are present and valid, otherwise false.
     */
    bool Validate(const CliOptionsConfig& config) const;
};

/**
 * @brief Convert command-line runner options to TensorFlow Lite backend options.
 *
 * @param  options  Command-line runner options to convert.
 * @return          TensorFlow Lite backend options.
 */
fwk::tflite::TfliteBackendOptions ToTfliteBackendOptions(const RunnerOptions& options);

} // namespace arm::app::common

#endif // RUNNER_OPTIONS_HPP
