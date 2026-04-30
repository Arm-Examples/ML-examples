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

#include "common/CliOptionSpecs.hpp"
#include "common/InputUtils.hpp"
#include "mlek/common/Classifier.hpp"
#include "mlek/use_case/img_class/ImgClassProcessing.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace arm::app::use_case::image_classification {

ImageClassificationApp::ImageClassificationApp(const common::RunnerOptions& options) :
    m_options(options)
{
    if (this->m_options.labelsPath.empty()) {
        throw std::runtime_error("image classification requires " +
                                 common::GetCliOptionName(common::CliOptionId::kLabels));
    }
    this->m_labels = image_classification::LoadLabels(this->m_options.labelsPath);

    fwk::iface::MemoryRegion computeBuffer{};
    fwk::iface::MemoryRegion modelBuffer{};

    auto backendOptions = common::ToTfliteBackendOptions(this->m_options);

    if (!this->m_model.Init(computeBuffer, modelBuffer, &backendOptions)) {
        throw std::runtime_error("failed to initialize TensorFlow Lite model");
    }
}

std::vector<std::string> LoadLabels(const std::string& labelsPath)
{
    std::ifstream labelsStream(labelsPath);
    if (!labelsStream.is_open()) {
        throw std::runtime_error("failed to open labels file: " + labelsPath);
    }

    std::vector<std::string> labels;
    std::string label;
    while (std::getline(labelsStream, label)) {
        labels.push_back(label);
    }

    if (labels.empty()) {
        throw std::runtime_error("labels file is empty: " + labelsPath);
    }

    return labels;
}

bool ImageClassificationApp::FillInputFromImage(const std::string& imagePath)
{
    try {
        if (this->m_model.GetNumInputs() != 1U) {
            throw std::runtime_error("image input requires exactly one input tensor");
        }

        const auto inputTensor = this->m_model.GetInputTensor(0);
        if (!inputTensor) {
            throw std::runtime_error("missing input tensor 0");
        }

        const common::DecodedImage image = common::ReadImageFile(imagePath);
        const auto [rows, cols]          = common::GetImageTensorRowsCols(*inputTensor);
        const int channels               = common::GetImageTensorChannels(*inputTensor);

        if (image.height != rows || image.width != cols) {
            throw std::runtime_error("image dimensions do not match input tensor shape");
        }

        if (channels != static_cast<int>(::arm::app::ImgClassPreProcess::kNumChannels)) {
            throw std::runtime_error("image classification requires a three-channel input tensor");
        }

        ::arm::app::ImgClassPreProcess preProcess(inputTensor);
        if (!preProcess.DoPreProcess(image.rgb.data(), image.rgb.size())) {
            throw std::runtime_error("image classification pre-processing failed");
        }
    } catch (const std::exception& err) {
        std::cerr << "Error while loading image input: " << err.what() << "\n";
        return false;
    }

    return true;
}

bool ImageClassificationApp::FillInput(const std::string& inputPath)
{
    if (this->m_options.inputMode == common::InputMode::kBinary) {
        return common::FillModelInputsFromBinaryFile(this->m_model, inputPath);
    }

    if (this->m_options.inputMode == common::InputMode::kImage) {
        return this->FillInputFromImage(inputPath);
    }

    std::cerr << "Error: unknown input mode\n";
    return false;
}

bool ImageClassificationApp::RunPostProcessing()
{
    if (this->m_model.GetNumOutputs() != 1U) {
        std::cerr << "Error: image classification requires exactly one output tensor\n";
        return false;
    }

    const auto outputTensor = this->m_model.GetOutputTensor(0);
    if (!outputTensor) {
        std::cerr << "Error: missing output tensor 0\n";
        return false;
    }

    ::arm::app::Classifier classifier;
    std::vector<::arm::app::ClassificationResult> results;
    ::arm::app::ImgClassPostProcess postProcess(outputTensor, classifier, this->m_labels, results);
    if (!postProcess.DoPostProcess()) {
        std::cerr << "Error: image classification post-processing failed\n";
        return false;
    }

    std::cout << "Postprocess: " << results.size() << " classifications\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        std::cout << "class[" << i << "] index=" << result.m_labelIdx
                  << " score=" << result.m_normalisedVal << " label=\"" << result.m_label
                  << "\"\n";
    }

    return true;
}

bool ImageClassificationApp::RunOne(const std::string& inputPath, const std::string& outputPath)
{
    if (!this->FillInput(inputPath)) {
        return false;
    }

    if (!this->m_model.RunInference()) {
        std::cerr << "Error: inference failed\n";
        return false;
    }

    if (!this->RunPostProcessing()) {
        return false;
    }

    return outputPath.empty() || common::WriteModelOutputsToFile(this->m_model, outputPath);
}

bool ImageClassificationApp::Run()
{
    if (this->m_options.profilingMode) {
        using namespace std::chrono;
        const std::string output =
            this->m_options.outputs.empty() ? std::string{} : this->m_options.outputs[0];

        const auto start = steady_clock::now();
        const bool ok    = this->RunOne(this->m_options.inputs[0], output);
        const auto end   = steady_clock::now();
        if (!ok) {
            return false;
        }

        const duration<long double, std::nano> total  = end - start;
        const duration<long double, std::nano> oneSec = seconds(1);
        const auto oldPrecision                       = std::cout.precision();
        std::cout << "Profiling report\n"
                  << std::setw(24) << "Inference path: " << total.count() << " ns ("
                  << std::fixed << std::setprecision(3) << oneSec / total << " inf/s)\n"
                  << std::resetiosflags(std::cout.flags());
        std::cout.precision(oldPrecision);
        return true;
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

} // namespace arm::app::use_case::image_classification
