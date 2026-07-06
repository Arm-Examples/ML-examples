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

#ifndef TFLITE_TENSOR_HPP
#define TFLITE_TENSOR_HPP

#include "mlek/fwk/iface/Tensor.hpp"

#include <tensorflow/lite/core/c/common.h>

#include <vector>

namespace arm::app::fwk::tflite {

/**
 * @brief Tensor interface backed by a TensorFlow Lite tensor.
 */
class TfliteTensor : public iface::TensorIface {
public:
    /**
     * @brief Create a tensor wrapper for a TensorFlow Lite tensor.
     *
     * @param  tensor  TensorFlow Lite tensor to wrap.
     */
    explicit TfliteTensor(TfLiteTensor* tensor);

    /**
     * @brief Destroy the tensor wrapper.
     */
    ~TfliteTensor() override = default;

    /**
     * @brief Get a mutable pointer to the tensor data.
     *
     * @return  Tensor data pointer.
     */
    void* GetData() override;

    /**
     * @brief Get the tensor storage size in bytes.
     *
     * @return  Tensor byte count.
     */
    size_t Bytes() override;

    /**
     * @brief Get the total number of tensor elements.
     *
     * @return  Product of all tensor shape dimensions.
     */
    size_t GetNumElements() override;

    /**
     * @brief Get the tensor data type.
     *
     * @return  Tensor data type mapped to the common interface.
     */
    iface::TensorType Type() override;

    /**
     * @brief Get the tensor layout.
     *
     * @return  Tensor layout.
     */
    iface::TensorLayout Layout() override;

    /**
     * @brief Get the tensor shape.
     *
     * @return  Tensor dimensions in TensorFlow Lite order.
     */
    std::vector<size_t> Shape() override;

    /**
     * @brief Get tensor quantization parameters.
     *
     * @return  Quantization scale and zero-point offset.
     */
    iface::QuantParams GetQuantParams() override;

private:
    TfLiteTensor* m_tensor{nullptr};
};

} // namespace arm::app::fwk::tflite

#endif // TFLITE_TENSOR_HPP
