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

#include "common/RunnerOptions.hpp"

#include "common/CliOptionSpecs.hpp"

namespace arm::app::common {
namespace {

std::string AcceptedInputHelp(const CliOptionsConfig& config)
{
    std::string help;
    if (SupportsOption(config, CliOptionId::kInputBin)) {
        help += GetCliOptionName(CliOptionId::kInputBin);
    }
    if (SupportsOption(config, CliOptionId::kInputImage)) {
        if (!help.empty()) {
            help += " or ";
        }
        help += GetCliOptionName(CliOptionId::kInputImage);
    }
    return help;
}

} // namespace

bool RunnerOptions::AddInput(const std::string& filePath, InputMode mode)
{
    if (this->inputMode == InputMode::kUnset) {
        this->inputMode = mode;
    }

    if (this->inputMode != mode) {
        std::cerr << "Error: binary and image inputs are mutually exclusive\n";
        return false;
    }

    this->inputs.push_back(filePath);
    return true;
}

bool RunnerOptions::Validate(const CliOptionsConfig& config) const
{
    if (this->modelPath.empty()) {
        std::cerr << "Error: missing model path. Use "
                  << GetCliOptionName(CliOptionId::kModel) << "\n";
        return false;
    }

    if (!std::filesystem::exists(this->modelPath)) {
        std::cerr << "Error: model file does not exist: " << this->modelPath << "\n";
        return false;
    }

    if (!SupportsOption(config, CliOptionId::kLabels) && !this->labelsPath.empty()) {
        std::cerr << "Error: labels are not supported by this app\n";
        return false;
    }

    if (!this->labelsPath.empty() && !std::filesystem::exists(this->labelsPath)) {
        std::cerr << "Error: labels file does not exist: " << this->labelsPath << "\n";
        return false;
    }

    if (this->inputMode == InputMode::kBinary &&
        !SupportsOption(config, CliOptionId::kInputBin)) {
        std::cerr << "Error: binary input is not supported by this app\n";
        return false;
    }

    if (this->inputMode == InputMode::kImage &&
        !SupportsOption(config, CliOptionId::kInputImage)) {
        std::cerr << "Error: image input is not supported by this app\n";
        return false;
    }

    if ((this->inputMode == InputMode::kUnset || this->inputs.empty()) &&
        !config.allowMissingInput) {
        const std::string acceptedInputs = AcceptedInputHelp(config);
        std::cerr << "Error: missing input";
        if (!acceptedInputs.empty()) {
            std::cerr << ". Use " << acceptedInputs;
        }
        std::cerr << "\n";
        return false;
    }

    for (const auto& input : this->inputs) {
        if (!std::filesystem::exists(input)) {
            std::cerr << "Error: input file does not exist: " << input << "\n";
            return false;
        }
    }

    if (this->useRandomInputIfMissing && this->inputs.empty() && this->outputs.size() > 1U) {
        std::cerr << "Error: random input mode accepts at most one "
                  << GetCliOptionName(CliOptionId::kOutput) << " file\n";
        return false;
    }

    if (!this->inputs.empty() && !this->outputs.empty() &&
        this->outputs.size() != this->inputs.size()) {
        std::cerr << "Error: number of " << GetCliOptionName(CliOptionId::kOutput)
                  << " files must match input count\n";
        return false;
    }

#if defined(ETHOS_U_NPU_ENABLED)
    if (!this->delegateLibPath.empty() && !std::filesystem::exists(this->delegateLibPath)) {
        std::cerr << "Error: delegate library does not exist: " << this->delegateLibPath << "\n";
        return false;
    }
#endif

    return true;
}

fwk::tflite::TfliteBackendOptions ToTfliteBackendOptions(const RunnerOptions& options)
{
    fwk::tflite::TfliteBackendOptions backendOptions{};
    backendOptions.modelPath          = options.modelPath;
    backendOptions.delegateLibPath    = options.delegateLibPath;
    backendOptions.deviceName         = options.deviceName;
    backendOptions.timeout            = options.timeout;
    backendOptions.enableCycleCounter = options.enableCycleCounter;
    backendOptions.useRefKernels      = options.useRefKernels;
    backendOptions.pmuEvents.assign(options.pmuEvents.begin(), options.pmuEvents.end());

    return backendOptions;
}

} // namespace arm::app::common
