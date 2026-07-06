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
#include "common/CliOptionSpecs.hpp"
#include "use_case/image_classification/ImageClassificationApp.hpp"

#include <algorithm>

namespace {

arm::app::common::CliOptionsConfig MakeCliOptionsConfig()
{
    auto optionIds = arm::app::common::CommonCliOptionIds();

    const auto inputBinIt = std::find(optionIds.begin(),
                                      optionIds.end(),
                                      arm::app::common::CliOptionId::kInputBin);
    optionIds.insert(inputBinIt, arm::app::common::CliOptionId::kLabels);

    const auto outputIt = std::find(optionIds.begin(),
                                    optionIds.end(),
                                    arm::app::common::CliOptionId::kOutput);
    optionIds.insert(outputIt, arm::app::common::CliOptionId::kInputImage);

    return {optionIds, false};
}

const arm::app::common::CliOptionsConfig kCliOptionsConfig = MakeCliOptionsConfig();

bool RunImageClassification(const arm::app::common::RunnerOptions& options)
{
    arm::app::use_case::image_classification::ImageClassificationApp app(options);
    return app.Run();
}

} // namespace

int main(int argc, char* argv[])
{
    return arm::app::common::RunCommandLineApp(
        argc,
        argv,
        RunImageClassification,
        kCliOptionsConfig);
}
