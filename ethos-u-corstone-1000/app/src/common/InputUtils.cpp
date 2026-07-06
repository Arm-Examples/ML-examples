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

#include "common/InputUtils.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace arm::app::common {
namespace {

using Image = DecodedImage;

uint16_t ReadLe16(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1U]) << 8U);
}

uint32_t ReadLe32(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<uint32_t>(bytes[offset + 3U]) << 24U);
}

int32_t ReadLeS32(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int32_t>(ReadLe32(bytes, offset));
}

std::string ReadPpmToken(std::ifstream& stream)
{
    std::string token;
    while (true) {
        if (!(stream >> token)) {
            throw std::runtime_error("failed to parse PPM header");
        }

        if (!token.empty() && token[0] == '#') {
            std::string discard;
            std::getline(stream, discard);
            continue;
        }

        return token;
    }
}

Image ReadPpmP6(const std::string& ppmPath)
{
    std::ifstream ppmStream(ppmPath, std::ios::binary);
    if (!ppmStream.is_open()) {
        throw std::runtime_error("failed to open PPM file: " + ppmPath);
    }

    const std::string magic = ReadPpmToken(ppmStream);
    if (magic != "P6") {
        throw std::runtime_error("only P6 PPM input is supported: " + ppmPath);
    }

    const int width  = std::stoi(ReadPpmToken(ppmStream));
    const int height = std::stoi(ReadPpmToken(ppmStream));
    const int maxVal = std::stoi(ReadPpmToken(ppmStream));
    if (width <= 0 || height <= 0 || maxVal != 255) {
        throw std::runtime_error("invalid PPM header values in: " + ppmPath);
    }

    ppmStream.get();

    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3U;
    std::vector<uint8_t> rgb(bytes);
    ppmStream.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(bytes));

    if (static_cast<size_t>(ppmStream.gcount()) != bytes) {
        throw std::runtime_error("PPM payload size mismatch in: " + ppmPath);
    }

    return Image{width, height, std::move(rgb)};
}

Image ReadBmp24(const std::string& bmpPath)
{
    std::ifstream bmpStream(bmpPath, std::ios::binary | std::ios::ate);
    if (!bmpStream.is_open()) {
        throw std::runtime_error("failed to open BMP file: " + bmpPath);
    }

    const auto fileSize = bmpStream.tellg();
    if (fileSize < 0) {
        throw std::runtime_error("failed to determine BMP file size: " + bmpPath);
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    bmpStream.seekg(0, std::ios_base::beg);
    bmpStream.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));

    if (static_cast<size_t>(bmpStream.gcount()) != bytes.size()) {
        throw std::runtime_error("BMP payload size mismatch in: " + bmpPath);
    }

    constexpr size_t kBmpFileHeaderBytes = 14;
    constexpr size_t kMinDibHeaderBytes  = 40;
    constexpr uint16_t kBmpMagic         = 0x4D42;
    constexpr uint16_t kRgbBitsPerPixel  = 24;
    constexpr uint32_t kNoCompression    = 0;
    if (bytes.size() < kBmpFileHeaderBytes + kMinDibHeaderBytes ||
        ReadLe16(bytes, 0) != kBmpMagic) {
        throw std::runtime_error("invalid BMP header: " + bmpPath);
    }

    const uint32_t pixelOffset = ReadLe32(bytes, 10);
    const uint32_t dibBytes    = ReadLe32(bytes, 14);
    const int32_t width        = ReadLeS32(bytes, 18);
    const int32_t height       = ReadLeS32(bytes, 22);
    const uint16_t planes      = ReadLe16(bytes, 26);
    const uint16_t bpp         = ReadLe16(bytes, 28);
    const uint32_t compression = ReadLe32(bytes, 30);

    if (dibBytes < kMinDibHeaderBytes || width <= 0 || height == 0 || planes != 1U ||
        bpp != kRgbBitsPerPixel || compression != kNoCompression) {
        throw std::runtime_error("only uncompressed 24-bit BMP input is supported: " + bmpPath);
    }

    const int absHeight = height < 0 ? -height : height;
    const size_t rowBytes =
        ((static_cast<size_t>(width) * static_cast<size_t>(bpp) + 31U) / 32U) * 4U;
    const size_t requiredBytes = static_cast<size_t>(pixelOffset) +
                                 rowBytes * static_cast<size_t>(absHeight);
    if (requiredBytes > bytes.size()) {
        throw std::runtime_error("BMP pixel data is truncated: " + bmpPath);
    }

    std::vector<uint8_t> rgb(static_cast<size_t>(width) * static_cast<size_t>(absHeight) * 3U);
    for (int row = 0; row < absHeight; ++row) {
        const int srcRow = height > 0 ? absHeight - 1 - row : row;
        const size_t src = static_cast<size_t>(pixelOffset) + rowBytes * static_cast<size_t>(srcRow);
        const size_t dst = static_cast<size_t>(row) * static_cast<size_t>(width) * 3U;
        for (int col = 0; col < width; ++col) {
            const size_t srcPixel    = src + static_cast<size_t>(col) * 3U;
            const size_t dstPixel    = dst + static_cast<size_t>(col) * 3U;
            rgb[dstPixel]            = bytes[srcPixel + 2U];
            rgb[dstPixel + 1U]       = bytes[srcPixel + 1U];
            rgb[dstPixel + 2U]       = bytes[srcPixel];
        }
    }

    return Image{width, absHeight, std::move(rgb)};
}

Image ReadImage(const std::string& imagePath)
{
    std::ifstream imageStream(imagePath, std::ios::binary);
    if (!imageStream.is_open()) {
        throw std::runtime_error("failed to open image file: " + imagePath);
    }

    std::array<char, 2> magic{};
    imageStream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (imageStream.gcount() != static_cast<std::streamsize>(magic.size())) {
        throw std::runtime_error("failed to read image header: " + imagePath);
    }

    if (magic[0] == 'P' && magic[1] == '6') {
        return ReadPpmP6(imagePath);
    }

    if (magic[0] == 'B' && magic[1] == 'M') {
        return ReadBmp24(imagePath);
    }

    throw std::runtime_error("unsupported image format: " + imagePath);
}

size_t TotalInputBytes(fwk::iface::Model& model)
{
    size_t bytes = 0;
    for (size_t i = 0; i < model.GetNumInputs(); ++i) {
        const auto tensor = model.GetInputTensor(i);
        if (!tensor) {
            throw std::runtime_error("missing input tensor " + std::to_string(i));
        }
        bytes += tensor->Bytes();
    }
    return bytes;
}

} // namespace

DecodedImage ReadImageFile(const std::string& imagePath)
{
    return ReadImage(imagePath);
}

std::pair<int, int> GetImageTensorRowsCols(fwk::iface::TensorIface& tensor)
{
    const auto shape = tensor.Shape();
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

int GetImageTensorChannels(fwk::iface::TensorIface& tensor)
{
    const auto shape = tensor.Shape();
    if (shape.size() < 3U) {
        throw std::runtime_error("input tensor does not have an image-like shape");
    }

    if (shape.size() == 4U && shape[3] > 4U) {
        return static_cast<int>(shape[1]);
    }

    return static_cast<int>(shape.back());
}

bool FillModelInputsFromBinaryFile(fwk::iface::Model& model, const std::string& inputPath)
{
    try {
        std::ifstream inputStream(inputPath, std::ios::binary | std::ios::ate);
        if (!inputStream.is_open()) {
            throw std::runtime_error("failed to open input file: " + inputPath);
        }

        const auto fileSize = inputStream.tellg();
        if (fileSize < 0) {
            throw std::runtime_error("failed to determine input file size: " + inputPath);
        }
        inputStream.seekg(0, std::ios_base::beg);

        const size_t expectedBytes = TotalInputBytes(model);
        if (static_cast<size_t>(fileSize) != expectedBytes) {
            throw std::runtime_error("input file size does not match total input tensor bytes");
        }

        for (size_t i = 0; i < model.GetNumInputs(); ++i) {
            const auto tensor = model.GetInputTensor(i);
            inputStream.read(reinterpret_cast<char*>(tensor->GetData()),
                             static_cast<std::streamsize>(tensor->Bytes()));
        }
    } catch (const std::exception& err) {
        std::cerr << "Error while loading binary input: " << err.what() << "\n";
        return false;
    }

    return true;
}

bool WriteModelOutputsToFile(fwk::iface::Model& model, const std::string& outputPath)
{
    try {
        std::ofstream outputStream(outputPath, std::ios::binary);
        if (!outputStream.is_open()) {
            throw std::runtime_error("failed to open output file: " + outputPath);
        }

        for (size_t i = 0; i < model.GetNumOutputs(); ++i) {
            const auto tensor = model.GetOutputTensor(i);
            if (!tensor) {
                throw std::runtime_error("missing output tensor " + std::to_string(i));
            }

            outputStream.write(reinterpret_cast<const char*>(tensor->GetData()),
                               static_cast<std::streamsize>(tensor->Bytes()));
        }
    } catch (const std::exception& err) {
        std::cerr << "Error while writing outputs: " << err.what() << "\n";
        return false;
    }

    return true;
}

} // namespace arm::app::common
