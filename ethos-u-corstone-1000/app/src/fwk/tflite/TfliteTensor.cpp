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

#include "fwk/tflite/TfliteTensor.hpp"

#include <cassert>

namespace arm::app::fwk::tflite {

TfliteTensor::TfliteTensor(TfLiteTensor* tensor) : m_tensor(tensor) {}

void* TfliteTensor::GetData()
{
    assert(this->m_tensor);
    return this->m_tensor->data.data;
}

size_t TfliteTensor::Bytes()
{
    assert(this->m_tensor);
    return this->m_tensor->bytes;
}

size_t TfliteTensor::GetNumElements()
{
    const auto shape = this->Shape();
    size_t elements  = 1;
    for (const auto dim : shape) {
        elements *= dim;
    }
    return elements;
}

iface::TensorType TfliteTensor::Type()
{
    assert(this->m_tensor);

    switch (this->m_tensor->type) {
    case kTfLiteUInt8:
        return iface::TensorType::UINT8;
    case kTfLiteInt8:
        return iface::TensorType::INT8;
    case kTfLiteInt16:
        return iface::TensorType::INT16;
    case kTfLiteInt32:
        return iface::TensorType::INT32;
    case kTfLiteFloat16:
        return iface::TensorType::FP16;
    case kTfLiteFloat32:
        return iface::TensorType::FP32;
    default:
        return iface::TensorType::INVALID;
    }
}

iface::TensorLayout TfliteTensor::Layout()
{
    return iface::TensorLayout::NHWC;
}

std::vector<size_t> TfliteTensor::Shape()
{
    assert(this->m_tensor);
    assert(this->m_tensor->dims);

    std::vector<size_t> shape;
    shape.reserve(static_cast<size_t>(this->m_tensor->dims->size));
    for (int i = 0; i < this->m_tensor->dims->size; ++i) {
        shape.push_back(static_cast<size_t>(this->m_tensor->dims->data[i]));
    }
    return shape;
}

iface::QuantParams TfliteTensor::GetQuantParams()
{
    iface::QuantParams params{0.0F, 0};
    assert(this->m_tensor);

    if (this->m_tensor->quantization.type == kTfLiteAffineQuantization) {
        auto* quantParams =
            reinterpret_cast<TfLiteAffineQuantization*>(this->m_tensor->quantization.params);
        if (quantParams != nullptr && quantParams->scale != nullptr &&
            quantParams->scale->size > 0) {
            params.scale = quantParams->scale->data[0];
        }
        if (quantParams != nullptr && quantParams->zero_point != nullptr &&
            quantParams->zero_point->size > 0) {
            params.offset = quantParams->zero_point->data[0];
        }
    } else if (this->m_tensor->params.scale != 0.0F) {
        params.scale  = this->m_tensor->params.scale;
        params.offset = this->m_tensor->params.zero_point;
    }

    return params;
}

} // namespace arm::app::fwk::tflite
