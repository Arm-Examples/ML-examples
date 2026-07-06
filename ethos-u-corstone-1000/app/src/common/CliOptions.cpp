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

#include "common/CliOptions.hpp"

#include "common/CliOptionSpecs.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <getopt.h>

#if defined(ETHOS_U_NPU_ENABLED)
#include <filesystem>
#include <stdexcept>
#endif

namespace arm::app::common {
namespace {

#if defined(ETHOS_U_NPU_ENABLED)
int PmuStrToInt(const char* str, size_t maxVal, const std::string& errorPrefix)
{
    int value = 0;

    try {
        value = std::stoi(str);
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Error: " + errorPrefix + " out of range value " + str);
    } catch (const std::exception&) {
        throw std::runtime_error("Error: " + errorPrefix + " non-numeric value " + str);
    }

    if (value < 0 || static_cast<size_t>(value) > maxVal) {
        throw std::runtime_error("Error: " + errorPrefix + " " + str + " is out of range [0 - " +
                                 std::to_string(maxVal) + "]");
    }

    return value;
}

bool ParsePmuArg(const char* counterIdxArg,
                 const char* eventIdArg,
                 std::array<int, RunnerOptions::kMaxNumCounters>& counters)
{
    constexpr size_t kMaxPmuEventId = 255;

    try {
        if (RunnerOptions::kMaxNumCounters == 0) {
            throw std::runtime_error("Error: PMU counters are not available in this build");
        }

        const int counterIdx = PmuStrToInt(counterIdxArg,
                                           RunnerOptions::kMaxNumCounters - 1,
                                           "invalid PMU counter index");
        const int eventId    = PmuStrToInt(eventIdArg, kMaxPmuEventId, "invalid PMU event ID");
        counters[static_cast<size_t>(counterIdx)] = eventId;
        return true;
    } catch (const std::exception& err) {
        std::cerr << err.what() << "\n";
        return false;
    }
}
#endif

int ToGetoptArgument(CliOptionArgument argument)
{
    return argument == CliOptionArgument::kRequired ? required_argument : no_argument;
}

void AddOption(std::vector<option>& options, std::string& optionString, const CliOptionSpec& spec)
{
    const int hasArg = ToGetoptArgument(spec.argument);
    options.push_back({spec.longOpt, hasArg, nullptr, spec.getoptValue});
    if (spec.aliasLongOpt != nullptr) {
        options.push_back({spec.aliasLongOpt, hasArg, nullptr, spec.getoptValue});
    }

    if (spec.shortOpt != '\0') {
        optionString.push_back(spec.shortOpt);
        if (spec.argument == CliOptionArgument::kRequired) {
            optionString.push_back(':');
        }
    }
}

const CliOptionSpec* FindEnabledOptionSpec(const CliOptionsConfig& config, int getoptValue)
{
    for (const auto optionId : config.optionIds) {
        const auto& spec = GetCliOptionSpec(optionId);
        if (spec.getoptValue == getoptValue) {
            return &spec;
        }
    }

    return nullptr;
}

} // namespace

void ShowHelp(const char* exe, const CliOptionsConfig& config)
{
    std::cerr << "Usage: " << exe << " [ARGS]\n\n"
              << "Arguments:\n";

    for (const auto optionId : config.optionIds) {
        const auto& spec = GetCliOptionSpec(optionId);
        std::cerr << spec.helpText;

#if defined(ETHOS_U_NPU_ENABLED)
        if (optionId == CliOptionId::kDevice) {
            std::cerr << "                    Default: " << RunnerOptions::kDefaultDevice << ".\n";
        } else if (optionId == CliOptionId::kTimeout) {
            std::cerr << "                    Default: " << RunnerOptions::kDefaultTimeout << ".\n";
        }
#endif
    }
}

bool ParseCliOptions(int argc, char* argv[], RunnerOptions& options, const CliOptionsConfig& config)
{
    std::vector<option> longOptions;
    std::string optionString;

    for (const auto optionId : config.optionIds) {
        AddOption(longOptions, optionString, GetCliOptionSpec(optionId));
    }
    longOptions.push_back({nullptr, 0, nullptr, 0});

    int opt = 0;
    optind = 1;
    while ((opt = getopt_long(argc, argv, optionString.c_str(), longOptions.data(), nullptr)) != -1) {
        const auto* spec = FindEnabledOptionSpec(config, opt);
        if (spec == nullptr) {
            return false;
        }

        switch (spec->id) {
        case CliOptionId::kModel:
            options.modelPath = optarg;
            break;
        case CliOptionId::kLabels:
            options.labelsPath = optarg;
            break;
        case CliOptionId::kInputBin:
            if (!options.AddInput(optarg, InputMode::kBinary)) {
                return false;
            }
            break;
        case CliOptionId::kInputImage:
            if (!options.AddInput(optarg, InputMode::kImage)) {
                return false;
            }
            break;
        case CliOptionId::kOutput:
            options.outputs.push_back(optarg);
            break;
        case CliOptionId::kRef:
            options.useRefKernels = true;
            break;
        case CliOptionId::kProfiling:
            options.profilingMode = true;
            break;
#if defined(ETHOS_U_NPU_ENABLED)
        case CliOptionId::kDevice:
            options.deviceName = optarg;
            break;
        case CliOptionId::kTimeout:
            options.timeout = optarg;
            break;
        case CliOptionId::kDelegateLib:
            options.delegateLibPath = std::filesystem::absolute(optarg);
            break;
        case CliOptionId::kPmu:
            if (optind >= argc) {
                std::cerr << "Missing PMU event ID for counter " << optarg << "\n";
                return false;
            }
            if (!ParsePmuArg(optarg, argv[optind], options.pmuEvents)) {
                return false;
            }
            optind++;
            break;
        case CliOptionId::kCycles:
            options.enableCycleCounter = true;
            break;
#endif
        case CliOptionId::kHelp:
            ShowHelp(argv[0], config);
            options.helpRequested = true;
            return true;
        default:
            return false;
        }
    }

    return true;
}

} // namespace arm::app::common
