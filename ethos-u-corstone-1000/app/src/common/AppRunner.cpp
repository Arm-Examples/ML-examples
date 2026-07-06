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

#include "common/AppRunner.hpp"

#include "common/CliOptions.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace arm::app::common {

int RunCommandLineApp(int argc,
                      char* argv[],
                      UseCaseRunner runUseCase,
                      const CliOptionsConfig& cliConfig,
                      OptionDefaults applyDefaults)
{
    RunnerOptions options;

    if (!ParseCliOptions(argc, argv, options, cliConfig)) {
        return EXIT_FAILURE;
    }

    if (options.helpRequested) {
        return EXIT_SUCCESS;
    }

    if (applyDefaults != nullptr) {
        applyDefaults(options);
    }

    if (!options.Validate(cliConfig)) {
        ShowHelp(argv[0], cliConfig);
        return EXIT_FAILURE;
    }

    try {
        return runUseCase(options) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << "\n";
        return EXIT_FAILURE;
    }
}

} // namespace arm::app::common
