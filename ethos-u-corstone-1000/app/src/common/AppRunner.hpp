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

#ifndef APP_RUNNER_HPP
#define APP_RUNNER_HPP

#include "common/RunnerOptions.hpp"

namespace arm::app::common {

/** Function that applies use-case-specific option defaults. */
using OptionDefaults = void (*)(RunnerOptions& options);

/** Function that runs a configured use case. */
using UseCaseRunner = bool (*)(const RunnerOptions& options);

/**
 * @brief Run a command-line use case application.
 *
 * @param argc             Number of command-line arguments.
 * @param argv             Command-line argument vector.
 * @param runUseCase       Function that executes the configured use case.
 * @param cliConfig        Command-line options supported by the use case.
 * @param applyDefaults    Optional function that applies use-case defaults.
 * @return                 Process exit code.
 */
int RunCommandLineApp(int argc,
                      char* argv[],
                      UseCaseRunner runUseCase,
                      const CliOptionsConfig& cliConfig,
                      OptionDefaults applyDefaults = nullptr);

} // namespace arm::app::common

#endif // APP_RUNNER_HPP
