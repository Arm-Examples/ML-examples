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

#ifndef CLI_OPTION_SPECS_HPP
#define CLI_OPTION_SPECS_HPP

#include "common/RunnerOptions.hpp"

#include <string>

namespace arm::app::common {

enum class CliOptionArgument {
    kNone,
    kRequired,
};

struct CliOptionSpec {
    CliOptionId id;
    const char* longOpt;
    const char* aliasLongOpt;
    char shortOpt;
    int getoptValue;
    CliOptionArgument argument;
    const char* helpText;
    InputMode inputMode;
};

const CliOptionSpec& GetCliOptionSpec(CliOptionId id);

std::vector<CliOptionId> CommonCliOptionIds();

bool SupportsOption(const CliOptionsConfig& config, CliOptionId id);

std::string GetCliOptionName(CliOptionId id);

} // namespace arm::app::common

#endif // CLI_OPTION_SPECS_HPP
