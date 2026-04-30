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

#ifndef OBJECT_DETECTION_APP_HPP
#define OBJECT_DETECTION_APP_HPP

#include "common/RunnerOptions.hpp"
#include "fwk/tflite/TfliteModel.hpp"

#include <string>
#include <utility>

namespace arm::app::use_case::object_detection {

/**
 * @brief Object detection application runner.
 */
class ObjectDetectionApp {
public:
    /**
     * @brief Create and initialize the object detection application.
     *
     * @param   options             Runtime options used by the application.
     * @throws  std::runtime_error  The TensorFlow Lite model cannot be initialized.
     */
    explicit ObjectDetectionApp(const common::RunnerOptions& options);

    /**
     * @brief Run object detection for the configured inputs.
     *
     * @return  True when all configured inputs run successfully, otherwise false.
     */
    bool Run();

private:
    /**
     * @brief Fill the model input tensor from one image file using MLEK preprocessing.
     *
     * @param  imagePath  Path to the image file.
     * @return            True when the image is loaded successfully, otherwise false.
     */
    bool FillInputFromImage(const std::string& imagePath);

    /**
     * @brief Fill the model input tensor from one configured input file.
     *
     * @param  inputPath  Path to the input file.
     * @return            True when the input is loaded successfully, otherwise false.
     */
    bool FillInput(const std::string& inputPath);

    /**
     * @brief Run inference and post-processing for one input.
     *
     * @param  inputPath   Path to the input file.
     * @param  outputPath  Optional path used to write raw model outputs.
     * @return             True when inference and optional output writing succeed.
     */
    bool RunOne(const std::string& inputPath, const std::string& outputPath);

    /**
     * @brief Run object detection post-processing and print detections.
     */
    void RunPostProcessing();

    /**
     * @brief Get the input tensor image dimensions.
     *
     * @return  Pair containing image rows and columns.
     * @throws  std::runtime_error  The model has no usable image input tensor.
     */
    std::pair<int, int> GetInputImageShape() const;

    const common::RunnerOptions& m_options;
    fwk::tflite::TfliteModel m_model{};
};

} // namespace arm::app::use_case::object_detection

#endif // OBJECT_DETECTION_APP_HPP
