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

#include "fwk/tflite/TfliteModel.hpp"
#include "fwk/tflite/TfliteTensor.hpp"
#include "mlek/log/log_macros.h"

#include <cstring>
#include <string>
#include <vector>

namespace arm::app::fwk::tflite {

#if defined(ETHOS_U_NPU_ENABLED)
void TfliteModel::ExternalDelegateDeleter::operator()(TfLiteDelegate* delegate) const
{
    if (delegate != nullptr) {
        TfLiteExternalDelegateDelete(delegate);
    }
}
#endif

bool TfliteModel::Init(iface::MemoryRegion& computeBuffer,
                       iface::MemoryRegion& nnModel,
                       const void* backendData)
{
    const auto* options = static_cast<const TfliteBackendOptions*>(backendData);
    const TfliteBackendOptions defaultOptions{};
    const auto& activeOptions = options != nullptr ? *options : defaultOptions;

    this->m_computeBuffer = computeBuffer;
    this->m_modelBuffer   = nnModel;

    if (!this->LoadModel(nnModel, activeOptions)) {
        return false;
    }

    if (!this->BuildInterpreter(activeOptions)) {
        return false;
    }

    if (this->m_interpreter->AllocateTensors() != kTfLiteOk) {
        printf_err("Failed to allocate TensorFlow Lite tensors\n");
        return false;
    }

    this->WrapTensors();
    if (this->m_input.empty() || this->m_output.empty()) {
        printf_err("TensorFlow Lite model has no input or output tensors\n");
        return false;
    }

    this->m_type = this->m_input[0]->Type();
    this->LogInterpreterInfo();
    this->m_inited = true;
    return true;
}

bool TfliteModel::LoadModel(const iface::MemoryRegion& nnModel,
                            const TfliteBackendOptions& options)
{
    if (!options.modelPath.empty()) {
        this->m_model = ::tflite::FlatBufferModel::VerifyAndBuildFromFile(
            options.modelPath.c_str());
    } else if (nnModel.data != nullptr && nnModel.size > 0U) {
        this->m_model = ::tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
            reinterpret_cast<const char*>(nnModel.data), nnModel.size);
    }

    if (!this->m_model) {
        printf_err("Failed to load TensorFlow Lite model\n");
        return false;
    }

    return true;
}

bool TfliteModel::BuildInterpreter(const TfliteBackendOptions& options)
{
    if (options.useRefKernels) {
        this->m_opResolver = std::make_unique<::tflite::ops::builtin::BuiltinRefOpResolver>();
    } else {
        this->m_opResolver = std::make_unique<::tflite::ops::builtin::BuiltinOpResolver>();
    }

    if (::tflite::InterpreterBuilder(*this->m_model, *this->m_opResolver)(
            &this->m_interpreter) != kTfLiteOk) {
        printf_err("Failed to create TensorFlow Lite interpreter\n");
        return false;
    }

#if defined(ETHOS_U_NPU_ENABLED)
    if (!this->SetupDelegates(options)) {
        return false;
    }
#else
    (void)options;
#endif

    return true;
}

void TfliteModel::WrapTensors()
{
    this->m_input.clear();
    this->m_output.clear();

    this->m_input.reserve(this->GetNumInputs());
    for (size_t i = 0; i < this->GetNumInputs(); ++i) {
        this->m_input.push_back(
            std::make_shared<TfliteTensor>(this->m_interpreter->input_tensor(i)));
    }

    this->m_output.reserve(this->GetNumOutputs());
    for (size_t i = 0; i < this->GetNumOutputs(); ++i) {
        this->m_output.push_back(
            std::make_shared<TfliteTensor>(this->m_interpreter->output_tensor(i)));
    }
}

#if defined(ETHOS_U_NPU_ENABLED)
bool TfliteModel::SetupDelegates(const TfliteBackendOptions& options)
{
    if (options.delegateLibPath.empty()) {
        return true;
    }

    static constexpr char kFlagDeviceName[]         = "device_name";
    static constexpr char kFlagTimeOut[]            = "timeout";
    static constexpr char kFlagEnableCycleCounter[] = "enable_cycle_counter";
    static constexpr char kFlagPmuEvent[]           = "pmu_event";

    TfLiteExternalDelegateOptions delegateOptions =
        TfLiteExternalDelegateOptionsDefault(options.delegateLibPath.c_str());
    std::vector<std::pair<std::string, std::string>> optionPairs;
    optionPairs.emplace_back(kFlagDeviceName, options.deviceName);
    optionPairs.emplace_back(kFlagTimeOut, options.timeout);

    if (options.enableCycleCounter) {
        optionPairs.emplace_back(kFlagEnableCycleCounter, "true");
    }

    for (size_t i = 0; i < options.pmuEvents.size(); ++i) {
        if (options.pmuEvents[i] <= 0) {
            continue;
        }
        optionPairs.emplace_back(std::string(kFlagPmuEvent) + std::to_string(i),
                                 std::to_string(options.pmuEvents[i]));
    }

    for (const auto& [key, value] : optionPairs) {
        delegateOptions.insert(&delegateOptions, key.c_str(), value.c_str());
    }

    this->m_externalDelegate.reset(TfLiteExternalDelegateCreate(&delegateOptions));
    if (!this->m_externalDelegate) {
        printf_err("Failed to create TensorFlow Lite external delegate\n");
        return false;
    }

    info("Created TensorFlow Lite external delegate: %s\n", options.delegateLibPath.c_str());
    if (this->m_interpreter->ModifyGraphWithDelegate(this->m_externalDelegate.get()) !=
        kTfLiteOk) {
        printf_err("Failed to apply TensorFlow Lite external delegate\n");
        return false;
    }

    return true;
}
#endif

std::shared_ptr<iface::TensorIface> TfliteModel::GetInputTensor(size_t index) const
{
    if (index >= this->m_input.size()) {
        return {};
    }
    return this->m_input[index];
}

std::shared_ptr<iface::TensorIface> TfliteModel::GetOutputTensor(size_t index) const
{
    if (index >= this->m_output.size()) {
        return {};
    }
    return this->m_output[index];
}

iface::TensorType TfliteModel::GetType() const
{
    return this->m_type;
}

std::vector<size_t> TfliteModel::GetInputShape(size_t index) const
{
    const auto tensor = this->GetInputTensor(index);
    return tensor ? tensor->Shape() : std::vector<size_t>{};
}

std::vector<size_t> TfliteModel::GetOutputShape(size_t index) const
{
    const auto tensor = this->GetOutputTensor(index);
    return tensor ? tensor->Shape() : std::vector<size_t>{};
}

size_t TfliteModel::GetNumInputs() const
{
    return this->m_interpreter ? this->m_interpreter->inputs().size() : 0U;
}

size_t TfliteModel::GetNumOutputs() const
{
    return this->m_interpreter ? this->m_interpreter->outputs().size() : 0U;
}

void TfliteModel::LogTensorInfo(std::shared_ptr<iface::TensorIface> tensor)
{
    if (!tensor) {
        printf_err("Invalid tensor\n");
        return;
    }

    info("\ttensor type is %s\n", iface::GetTensorDataTypeName(tensor->Type()));
    info("\ttensor occupies %zu bytes\n", tensor->Bytes());
    const auto shape = tensor->Shape();
    for (size_t i = 0; i < shape.size(); ++i) {
        info("\t\t%zu: %zu\n", i, shape[i]);
    }
}

void TfliteModel::LogInterpreterInfo()
{
    info("Model input tensors:\n");
    for (auto& input : this->m_input) {
        this->LogTensorInfo(input);
    }

    info("Model output tensors:\n");
    for (auto& output : this->m_output) {
        this->LogTensorInfo(output);
    }
}

bool TfliteModel::IsInited() const
{
    return this->m_inited;
}

bool TfliteModel::IsDataSigned() const
{
    switch (this->GetType()) {
    case iface::TensorType::INT8:
        [[fallthrough]];
    case iface::TensorType::INT16:
        [[fallthrough]];
    case iface::TensorType::INT32:
        return true;
    default:
        return false;
    }
}

bool TfliteModel::ContainsEthosUOperator() const
{
    if (!this->m_model || this->m_model->GetModel() == nullptr ||
        this->m_model->GetModel()->operator_codes() == nullptr) {
        return false;
    }

    const auto* opcodes = this->m_model->GetModel()->operator_codes();
    for (uint32_t i = 0; i < opcodes->size(); ++i) {
        const auto* opcode = opcodes->Get(i);
        if (opcode == nullptr || opcode->builtin_code() != ::tflite::BuiltinOperator_CUSTOM ||
            opcode->custom_code() == nullptr) {
            continue;
        }

        if (std::strcmp(opcode->custom_code()->c_str(), "ethos-u") == 0) {
            return true;
        }
    }

    return false;
}

bool TfliteModel::RunInference()
{
    if (!this->m_interpreter) {
        printf_err("No TensorFlow Lite interpreter\n");
        return false;
    }

    if (this->m_interpreter->Invoke() != kTfLiteOk) {
        printf_err("TensorFlow Lite Invoke failed\n");
        return false;
    }

    return true;
}

const iface::MemoryRegion& TfliteModel::GetComputeBuffer() const
{
    return this->m_computeBuffer;
}

const iface::MemoryRegion& TfliteModel::GetModelBuffer() const
{
    return this->m_modelBuffer;
}

} // namespace arm::app::fwk::tflite
