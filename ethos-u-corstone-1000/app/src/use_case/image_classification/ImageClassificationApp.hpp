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

#ifndef IMAGE_CLASSIFICATION_APP_HPP
#define IMAGE_CLASSIFICATION_APP_HPP

#include "common/RunnerOptions.hpp"
#include "fwk/tflite/TfliteModel.hpp"

#include <string>
#include <vector>

namespace arm::app::use_case::image_classification {

/**
 * @brief Load one label per line from a labels text file.
 *
 * @param   labelsPath          Path to the labels file.
 * @return                      Labels in file order.
 * @throws  std::runtime_error  The labels file cannot be read or is empty.
 */
std::vector<std::string> LoadLabels(const std::string& labelsPath);

/**
 * @brief Image classification application runner.
 */
class ImageClassificationApp {
public:
    /**
     * @brief Create and initialize the image classification application.
     *
     * @param   options             Runtime options used by the application.
     * @throws  std::runtime_error  The labels or TensorFlow Lite model cannot be initialized.
     */
    explicit ImageClassificationApp(const common::RunnerOptions& options);

    /**
     * @brief Run image classification for the configured inputs.
     *
     * @return  True when all configured inputs run successfully, otherwise false.
     */
    bool Run();

private:
    /**
     * @brief Fill the model input tensor from one configured input file.
     *
     * @param  inputPath  Path to the input file.
     * @return            True when the input is loaded successfully, otherwise false.
     */
    bool FillInput(const std::string& inputPath);

    /**
     * @brief Decode an image and run MLEK image classification pre-processing.
     *
     * @param  imagePath  Path to the image file.
     * @return            True when pre-processing succeeds, otherwise false.
     */
    bool FillInputFromImage(const std::string& imagePath);

    /**
     * @brief Run inference and post-processing for one input.
     *
     * @param  inputPath   Path to the input file.
     * @param  outputPath  Optional path used to write raw model outputs.
     * @return             True when inference, post-processing and optional output writing succeed.
     */
    bool RunOne(const std::string& inputPath, const std::string& outputPath);

    /**
     * @brief Run MLEK image classification post-processing and print top classes.
     *
     * @return  True when post-processing succeeds, otherwise false.
     */
    bool RunPostProcessing();

    const common::RunnerOptions& m_options;
    fwk::tflite::TfliteModel m_model{};
    std::vector<std::string> m_labels{};
};

} // namespace arm::app::use_case::image_classification

#endif // IMAGE_CLASSIFICATION_APP_HPP
