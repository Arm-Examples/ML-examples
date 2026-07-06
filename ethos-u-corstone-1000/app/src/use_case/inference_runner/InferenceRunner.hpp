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

#ifndef INFERENCE_RUNNER_HPP
#define INFERENCE_RUNNER_HPP

#include "common/RunnerOptions.hpp"
#include "fwk/tflite/TfliteModel.hpp"

namespace arm::app::use_case::inference_runner {

/**
 * @brief Generic TensorFlow Lite inference runner.
 */
class InferenceRunner {
public:
    /**
     * @brief Create and initialize the inference runner.
     *
     * @param   options             Runtime options used by the runner.
     * @throws  std::runtime_error  The TensorFlow Lite model cannot be initialized.
     */
    explicit InferenceRunner(const common::RunnerOptions& options);

    /**
     * @brief Run inference for the configured inputs.
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
     * @brief Fill all model input tensors with generated random data.
     *
     * @return  True when all model inputs are populated, otherwise false.
     */
    bool FillRandomInputs();

    /**
     * @brief Run inference for one input.
     *
     * @param  inputPath   Path to the input file.
     * @param  outputPath  Optional path used to write raw model outputs.
     * @return             True when inference and optional output writing succeed.
     */
    bool RunOne(const std::string& inputPath, const std::string& outputPath);

    /**
     * @brief Run inference with generated random input data.
     *
     * @param  outputPath  Optional path used to write raw model outputs.
     * @return             True when inference and optional output writing succeed.
     */
    bool RunRandom(const std::string& outputPath);

    common::RunnerOptions m_options;
    fwk::tflite::TfliteModel m_model{};
};

} // namespace arm::app::use_case::inference_runner

#endif // INFERENCE_RUNNER_HPP
