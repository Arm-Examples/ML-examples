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

#ifndef CLI_OPTIONS_HPP
#define CLI_OPTIONS_HPP

#include "common/RunnerOptions.hpp"

namespace arm::app::common {

/**
 * @brief Print command-line usage information.
 *
 * @param  exe  Executable name to show in the usage line.
 */
void ShowHelp(const char* exe, const CliOptionsConfig& config);

/**
 * @brief Parse command-line arguments into runner options.
 *
 * @param  argc     Number of command-line arguments.
 * @param  argv     Command-line argument vector.
 * @param  options  Runner options populated from parsed arguments.
 * @return          True when parsing succeeds, otherwise false.
 */
bool ParseCliOptions(int argc, char* argv[], RunnerOptions& options, const CliOptionsConfig& config);

} // namespace arm::app::common

#endif // CLI_OPTIONS_HPP
