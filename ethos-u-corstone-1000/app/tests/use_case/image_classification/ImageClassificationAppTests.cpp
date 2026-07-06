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

#include "use_case/image_classification/ImageClassificationApp.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

namespace ic = arm::app::use_case::image_classification;

std::string g_testDataDir;

std::filesystem::path TestDataPath(const std::string& filename)
{
    return std::filesystem::path{g_testDataDir} / filename;
}

TEST_CASE("Image classification labels are loaded in file order",
          "[direct-drive][image-classification]")
{
    const auto labels = ic::LoadLabels(TestDataPath("labels.txt").string());

    REQUIRE(labels == std::vector<std::string>{"zero", "one", "two"});
}

TEST_CASE("Image classification label loading rejects empty files",
          "[direct-drive][image-classification]")
{
    REQUIRE_THROWS_WITH(ic::LoadLabels(TestDataPath("empty_labels.txt").string()),
                        Catch::Matchers::ContainsSubstring("labels file is empty"));
}

TEST_CASE("Image classification label loading rejects missing files",
          "[direct-drive][image-classification]")
{
    REQUIRE_THROWS_WITH(ic::LoadLabels(TestDataPath("missing_labels.txt").string()),
                        Catch::Matchers::ContainsSubstring("failed to open labels file"));
}

} // namespace

int main(int argc, char* argv[])
{
    Catch::Session session;

    auto cli = session.cli() |
        Catch::Clara::Opt(g_testDataDir, "path")["--test-data-dir"]("test data directory");
    session.cli(cli);

    const int result = session.applyCommandLine(argc, argv);
    if (result != 0) {
        return result;
    }

    if (g_testDataDir.empty()) {
        std::cerr << "Error: --test-data-dir is required\n";
        return 1;
    }

    return session.run();
}
