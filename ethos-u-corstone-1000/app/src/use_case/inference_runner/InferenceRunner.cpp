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
#include "common/InputUtils.hpp"
#include "use_case/inference_runner/InferenceRunner.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace arm::app::use_case::inference_runner {
namespace {

template <typename TensorDataType, typename DistributionDataType>
void FillRandomIntegralData(fwk::iface::TensorIface& tensor,
                            std::mt19937& generator,
                            DistributionDataType min,
                            DistributionDataType max)
{
    std::uniform_int_distribution<DistributionDataType> distribution(min, max);
    auto* data = tensor.GetData<TensorDataType>();
    for (size_t i = 0; i < tensor.GetNumElements(); ++i) {
        data[i] = static_cast<TensorDataType>(distribution(generator));
    }
}

template <typename TensorDataType>
void FillRandomRealData(fwk::iface::TensorIface& tensor,
                        std::mt19937& generator,
                        TensorDataType min,
                        TensorDataType max)
{
    std::uniform_real_distribution<TensorDataType> distribution(min, max);
    auto* data = tensor.GetData<TensorDataType>();
    for (size_t i = 0; i < tensor.GetNumElements(); ++i) {
        data[i] = distribution(generator);
    }
}

void FillRandomData(fwk::iface::TensorIface& tensor, std::mt19937& generator)
{
    switch (tensor.Type()) {
    case fwk::iface::TensorType::INT8:
        FillRandomIntegralData<int8_t, int>(
            tensor,
            generator,
            std::numeric_limits<int8_t>::min(),
            std::numeric_limits<int8_t>::max());
        return;
    case fwk::iface::TensorType::UINT8:
        FillRandomIntegralData<uint8_t, int>(
            tensor,
            generator,
            std::numeric_limits<uint8_t>::min(),
            std::numeric_limits<uint8_t>::max());
        return;
    case fwk::iface::TensorType::INT16:
        FillRandomIntegralData<int16_t, int>(
            tensor,
            generator,
            std::numeric_limits<int16_t>::min(),
            std::numeric_limits<int16_t>::max());
        return;
    case fwk::iface::TensorType::INT32:
        FillRandomIntegralData<int32_t, int32_t>(
            tensor,
            generator,
            std::numeric_limits<int32_t>::min(),
            std::numeric_limits<int32_t>::max());
        return;
    case fwk::iface::TensorType::FP16: {
        constexpr uint16_t kMaxFiniteLessThanOne = 0x3BFFU;
        FillRandomIntegralData<uint16_t, uint16_t>(tensor, generator, 0U, kMaxFiniteLessThanOne);
        return;
    }
    case fwk::iface::TensorType::FP32:
        FillRandomRealData<float>(tensor, generator, 0.0F, 1.0F);
        return;
    default:
        throw std::runtime_error("unsupported input tensor type for random input");
    }
}

} // namespace

InferenceRunner::InferenceRunner(const common::RunnerOptions& options) : m_options(options)
{
    fwk::iface::MemoryRegion computeBuffer{};
    fwk::iface::MemoryRegion modelBuffer{};

    auto backendOptions = common::ToTfliteBackendOptions(this->m_options);

    if (!this->m_model.Init(computeBuffer, modelBuffer, &backendOptions)) {
        throw std::runtime_error("failed to initialize TensorFlow Lite model");
    }
}

bool InferenceRunner::FillInput(const std::string& inputPath)
{
    if (this->m_options.inputMode == common::InputMode::kBinary) {
        return common::FillModelInputsFromBinaryFile(this->m_model, inputPath);
    }

    if (this->m_options.inputMode == common::InputMode::kImage) {
        std::cerr << "Error: inference runner does not support image input; use "
                  << common::GetCliOptionName(common::CliOptionId::kInputBin)
                  << " or omit input for random data\n";
        return false;
    }

    std::cerr << "Error: unknown input mode\n";
    return false;
}

bool InferenceRunner::FillRandomInputs()
{
    try {
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());

        for (size_t i = 0; i < this->m_model.GetNumInputs(); ++i) {
            const auto tensor = this->m_model.GetInputTensor(i);
            if (!tensor) {
                throw std::runtime_error("missing input tensor " + std::to_string(i));
            }
            FillRandomData(*tensor, generator);
        }
    } catch (const std::exception& err) {
        std::cerr << "Error while generating random input: " << err.what() << "\n";
        return false;
    }

    return true;
}

bool InferenceRunner::RunOne(const std::string& inputPath, const std::string& outputPath)
{
    if (!this->FillInput(inputPath)) {
        return false;
    }

    if (!this->m_model.RunInference()) {
        std::cerr << "Error: inference failed\n";
        return false;
    }

    if (!outputPath.empty() && !common::WriteModelOutputsToFile(this->m_model, outputPath)) {
        return false;
    }

    return true;
}

bool InferenceRunner::RunRandom(const std::string& outputPath)
{
    if (!this->FillRandomInputs()) {
        return false;
    }

    if (!this->m_model.RunInference()) {
        std::cerr << "Error: inference failed\n";
        return false;
    }

    if (!outputPath.empty() && !common::WriteModelOutputsToFile(this->m_model, outputPath)) {
        return false;
    }

    return true;
}

bool InferenceRunner::Run()
{
    const bool useRandomInput = this->m_options.inputs.empty();

    if (this->m_options.profilingMode) {
        using namespace std::chrono;
        const std::string output =
            this->m_options.outputs.empty() ? std::string{} : this->m_options.outputs[0];

        const auto start = steady_clock::now();
        const bool ok =
            useRandomInput ? this->RunRandom(output) : this->RunOne(this->m_options.inputs[0], output);
        const auto end = steady_clock::now();
        if (!ok) {
            return false;
        }

        const duration<long double, std::nano> total = end - start;
        const duration<long double, std::nano> oneSec = seconds(1);
        const auto oldPrecision = std::cout.precision();
        std::cout << "Profiling report\n"
                  << std::setw(24) << "Inference path: " << total.count() << " ns ("
                  << std::fixed << std::setprecision(3) << oneSec / total << " inf/s)\n"
                  << std::resetiosflags(std::cout.flags());
        std::cout.precision(oldPrecision);
        return true;
    }

    if (useRandomInput) {
        const std::string output =
            this->m_options.outputs.empty() ? std::string{} : this->m_options.outputs[0];
        return this->RunRandom(output);
    }

    for (size_t i = 0; i < this->m_options.inputs.size(); ++i) {
        const std::string output =
            this->m_options.outputs.empty() ? std::string{} : this->m_options.outputs[i];
        if (!this->RunOne(this->m_options.inputs[i], output)) {
            return false;
        }
    }

    return true;
}

} // namespace arm::app::use_case::inference_runner
