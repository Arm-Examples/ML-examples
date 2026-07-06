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

#include "common/CliOptionSpecs.hpp"

#include <algorithm>
#include <stdexcept>

namespace arm::app::common {
namespace {

constexpr char kNoShortOpt = '\0';

const std::vector<CliOptionSpec> kCliOptionSpecs{
        {CliOptionId::kHelp,
         "help",
         nullptr,
         'h',
         'h',
         CliOptionArgument::kNone,
         "    -h --help       Print this help message.\n",
         InputMode::kUnset},
        {CliOptionId::kModel,
         "model",
         nullptr,
         'm',
         'm',
         CliOptionArgument::kRequired,
         "    -m --model      TensorFlow Lite model path.\n",
         InputMode::kUnset},
        {CliOptionId::kLabels,
         "labels",
         nullptr,
         'L',
         'L',
         CliOptionArgument::kRequired,
         "    -L --labels     Labels text file path.\n",
         InputMode::kUnset},
        {CliOptionId::kInputBin,
         "input-bin",
         nullptr,
         'B',
         'B',
         CliOptionArgument::kRequired,
         "    -B --input-bin    Raw concatenated input tensor data.\n"
         "                    Can be passed multiple times.\n",
         InputMode::kBinary},
        {CliOptionId::kInputImage,
         "input-image",
         "input-ppm",
         'I',
         'I',
         CliOptionArgument::kRequired,
         "    -I --input-image  Input image in P6 PPM or 24-bit BMP format.\n"
         "                    Can be passed multiple times.\n",
         InputMode::kImage},
        {CliOptionId::kOutput,
         "output",
         nullptr,
         'o',
         'o',
         CliOptionArgument::kRequired,
         "    -o --output     File to write concatenated output tensors to.\n"
         "                    Optional. If repeated, count must match inputs.\n",
         InputMode::kUnset},
        {CliOptionId::kRef,
         "ref",
         nullptr,
         'r',
         'r',
         CliOptionArgument::kNone,
         "    -r --ref        Use TensorFlow Lite reference kernels.\n",
         InputMode::kUnset},
        {CliOptionId::kProfiling,
         "profiling",
         nullptr,
         kNoShortOpt,
         'X',
         CliOptionArgument::kNone,
         "       --profiling  Run one inference and print timing.\n",
         InputMode::kUnset},
#if defined(ETHOS_U_NPU_ENABLED)
        {CliOptionId::kCycles,
         "cycles",
         nullptr,
         'C',
         'C',
         CliOptionArgument::kNone,
         "    -C --cycles     Enable delegate cycle counter.\n",
         InputMode::kUnset},
        {CliOptionId::kDevice,
         "device",
         nullptr,
         'd',
         'd',
         CliOptionArgument::kRequired,
         "    -d --device     Ethos-U device path.\n",
         InputMode::kUnset},
        {CliOptionId::kTimeout,
         "timeout",
         nullptr,
         't',
         't',
         CliOptionArgument::kRequired,
         "    -t --timeout    Delegate timeout in ns.\n",
         InputMode::kUnset},
        {CliOptionId::kDelegateLib,
         "lib",
         nullptr,
         'l',
         'l',
         CliOptionArgument::kRequired,
         "    -l --lib        Path to libethosu_op_delegate.so.\n",
         InputMode::kUnset},
        {CliOptionId::kPmu,
         "pmu",
         nullptr,
         'P',
         'P',
         CliOptionArgument::kRequired,
         "    -P --pmu        Counter index followed by event id.\n"
         "                    Can be passed multiple times.\n",
         InputMode::kUnset},
#endif
};

} // namespace

const CliOptionSpec& GetCliOptionSpec(CliOptionId id)
{
    const auto it = std::find_if(kCliOptionSpecs.begin(),
                                 kCliOptionSpecs.end(),
                                 [id](const CliOptionSpec& spec) { return spec.id == id; });
    if (it == kCliOptionSpecs.end()) {
        throw std::logic_error("Unknown CLI option ID");
    }

    return *it;
}

std::vector<CliOptionId> CommonCliOptionIds()
{
    return {
        CliOptionId::kHelp,
        CliOptionId::kModel,
        CliOptionId::kInputBin,
        CliOptionId::kOutput,
        CliOptionId::kRef,
        CliOptionId::kProfiling,
#if defined(ETHOS_U_NPU_ENABLED)
        CliOptionId::kDevice,
        CliOptionId::kDelegateLib,
        CliOptionId::kTimeout,
        CliOptionId::kPmu,
        CliOptionId::kCycles,
#endif
    };
}

bool SupportsOption(const CliOptionsConfig& config, CliOptionId id)
{
    return std::find(config.optionIds.begin(), config.optionIds.end(), id) != config.optionIds.end();
}

std::string GetCliOptionName(CliOptionId id)
{
    return std::string("--") + GetCliOptionSpec(id).longOpt;
}

} // namespace arm::app::common
