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

#include "use_case/object_detection/ObjectDetectionApp.hpp"

#include "common/InputUtils.hpp"
#include "mlek/use_case/object_detection/DetectorPostProcessing.hpp"
#include "mlek/use_case/object_detection/DetectorPreProcessing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace arm::app::use_case::object_detection {
namespace {

constexpr std::array<float, 6> kAnchor1{38.0F, 77.0F, 47.0F, 97.0F, 61.0F, 126.0F};
constexpr std::array<float, 6> kAnchor2{14.0F, 26.0F, 19.0F, 37.0F, 28.0F, 55.0F};

::arm::app::object_detection::PostProcessParams GetDefaultPostProcessParams(int rows, int cols)
{
    if (rows != cols) {
        throw std::runtime_error("object detection post-processing requires a square image");
    }

    ::arm::app::object_detection::PostProcessParams params{};
    params.inputImgRows     = rows;
    params.inputImgCols     = cols;
    params.originalImageSize = rows;
    params.anchor1          = kAnchor1.data();
    params.anchor2          = kAnchor2.data();
    params.threshold        = 0.5F;
    params.nms              = 0.45F;
    params.numClasses       = 1;
    params.topN             = 0;
    return params;
}

void ValidateOutputTensor(const std::shared_ptr<fwk::iface::TensorIface>& tensor,
                          const char* name)
{
    if (!tensor) {
        throw std::runtime_error(std::string("missing output tensor: ") + name);
    }

    if (tensor->Type() != fwk::iface::TensorType::INT8) {
        throw std::runtime_error(std::string("expected int8 output tensor for ") + name);
    }

    if (tensor->GetData<int8_t>() == nullptr) {
        throw std::runtime_error(std::string("null output tensor data pointer: ") + name);
    }
}

} // namespace

ObjectDetectionApp::ObjectDetectionApp(const common::RunnerOptions& options) : m_options(options)
{
    fwk::iface::MemoryRegion computeBuffer{};
    fwk::iface::MemoryRegion modelBuffer{};

    auto backendOptions = common::ToTfliteBackendOptions(this->m_options);

    if (!this->m_model.Init(computeBuffer, modelBuffer, &backendOptions)) {
        throw std::runtime_error("failed to initialize TensorFlow Lite model");
    }
}

bool ObjectDetectionApp::FillInputFromImage(const std::string& imagePath)
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

        if (channels != 1 && channels != 3) {
            throw std::runtime_error("image input supports only 1 or 3 input channels");
        }

        const auto tensorType = inputTensor->Type();
        if (tensorType != fwk::iface::TensorType::UINT8 &&
            tensorType != fwk::iface::TensorType::INT8) {
            throw std::runtime_error(
                "object detection image input supports only uint8 or int8 tensors");
        }

        const bool rgb2Gray     = channels == 1;
        const bool convertToInt8 = tensorType == fwk::iface::TensorType::INT8;

        ::arm::app::DetectorPreProcess preProcess(inputTensor, rgb2Gray, convertToInt8);
        if (!preProcess.DoPreProcess(image.rgb.data(), image.rgb.size())) {
            throw std::runtime_error("object detection pre-processing failed");
        }
    } catch (const std::exception& err) {
        std::cerr << "Error while loading image input: " << err.what() << "\n";
        return false;
    }

    return true;
}

bool ObjectDetectionApp::FillInput(const std::string& inputPath)
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

std::pair<int, int> ObjectDetectionApp::GetInputImageShape() const
{
    if (this->m_model.GetNumInputs() == 0U) {
        throw std::runtime_error("model has no input tensors");
    }

    const auto input = this->m_model.GetInputTensor(0);
    if (!input) {
        throw std::runtime_error("missing input tensor 0");
    }

    const auto shape = input->Shape();
    if (shape.size() < 3U) {
        throw std::runtime_error("input tensor does not have an image-like shape");
    }

    if (shape.size() == 4U) {
        if (shape[3] <= 4U) {
            return {static_cast<int>(shape[1]), static_cast<int>(shape[2])};
        }
        return {static_cast<int>(shape[2]), static_cast<int>(shape[3])};
    }

    return {static_cast<int>(shape[shape.size() - 3U]),
            static_cast<int>(shape[shape.size() - 2U])};
}

void ObjectDetectionApp::RunPostProcessing()
{
    const auto [rows, cols] = this->GetInputImageShape();
    const auto params       = GetDefaultPostProcessParams(rows, cols);

    if (this->m_model.GetNumOutputs() < 2U) {
        throw std::runtime_error("expected at least two output tensors for detection post-processing");
    }

    const auto output0 = this->m_model.GetOutputTensor(0);
    const auto output1 = this->m_model.GetOutputTensor(1);
    ValidateOutputTensor(output0, "output0");
    ValidateOutputTensor(output1, "output1");

    std::vector<::arm::app::object_detection::DetectionResult> detections;
    ::arm::app::DetectorPostProcess postProcess(output0, output1, detections, params);
    if (!postProcess.DoPostProcess()) {
        throw std::runtime_error("object detection post-processing failed");
    }

    std::cout << "Postprocess: " << detections.size() << " detections\n";
    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& detection = detections[i];
        std::cout << "det[" << i << "] class=0"
                  << " score=" << detection.m_normalisedVal << " x=" << detection.m_x0
                  << " y=" << detection.m_y0 << " w=" << detection.m_w
                  << " h=" << detection.m_h << "\n";
    }
}

bool ObjectDetectionApp::RunOne(const std::string& inputPath, const std::string& outputPath)
{
    if (!this->FillInput(inputPath)) {
        return false;
    }

    if (!this->m_model.RunInference()) {
        std::cerr << "Error: inference failed\n";
        return false;
    }

    try {
        this->RunPostProcessing();
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << "\n";
        return false;
    }

    return outputPath.empty() || common::WriteModelOutputsToFile(this->m_model, outputPath);
}

bool ObjectDetectionApp::Run()
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

} // namespace arm::app::use_case::object_detection
