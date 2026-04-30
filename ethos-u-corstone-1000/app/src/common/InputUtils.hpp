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

#ifndef INPUT_UTILS_HPP
#define INPUT_UTILS_HPP

#include "mlek/fwk/iface/Model.hpp"
#include "mlek/fwk/iface/Tensor.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace arm::app::common {

/**
 * @brief RGB image decoded from a supported input file.
 */
struct DecodedImage {
    int width{0};
    int height{0};
    std::vector<uint8_t> rgb{};
};

/**
 * @brief Decode a supported RGB image file.
 *
 * Supported images are P6 PPM and uncompressed 24-bit BMP files.
 *
 * @param  imagePath  Path to the image file.
 * @return            Decoded image in row-major RGB order.
 */
DecodedImage ReadImageFile(const std::string& imagePath);

/**
 * @brief Get image rows and columns from an image-like tensor.
 *
 * @param  tensor  Tensor whose shape is inspected.
 * @return         Pair of rows and columns.
 */
std::pair<int, int> GetImageTensorRowsCols(fwk::iface::TensorIface& tensor);

/**
 * @brief Get image channel count from an image-like tensor.
 *
 * @param  tensor  Tensor whose shape is inspected.
 * @return         Number of channels.
 */
int GetImageTensorChannels(fwk::iface::TensorIface& tensor);

/**
 * @brief Load raw binary input data into all model input tensors.
 *
 * The input file must contain the concatenated bytes for every input tensor in
 * model order.
 *
 * @param  model      Model whose input tensors are populated.
 * @param  inputPath  Path to the raw binary input file.
 * @return            True when the input file is loaded successfully, otherwise false.
 */
bool FillModelInputsFromBinaryFile(fwk::iface::Model& model, const std::string& inputPath);

/**
 * @brief Write all model output tensors to a binary file.
 *
 * Output tensors are written as concatenated raw bytes in model output order.
 *
 * @param  model       Model whose output tensors are written.
 * @param  outputPath  Path to the output file.
 * @return             True when the output file is written successfully, otherwise false.
 */
bool WriteModelOutputsToFile(fwk::iface::Model& model, const std::string& outputPath);

} // namespace arm::app::common

#endif // INPUT_UTILS_HPP
