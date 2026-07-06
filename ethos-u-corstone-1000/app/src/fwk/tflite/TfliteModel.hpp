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

#ifndef TFLITE_MODEL_HPP
#define TFLITE_MODEL_HPP

#include "fwk/tflite/TfliteBackendOptions.hpp"
#include "mlek/fwk/iface/Model.hpp"

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/kernels/register_ref.h>
#include <tensorflow/lite/model.h>

#if defined(ETHOS_U_NPU_ENABLED)
#include <tensorflow/lite/delegates/external/external_delegate.h>
#endif

#include <memory>
#include <vector>

namespace arm::app::fwk::tflite {

/**
 * @brief Model interface backed by a TensorFlow Lite interpreter.
 */
class TfliteModel : public iface::Model {
public:
    /**
     * @brief Create an uninitialized TensorFlow Lite model wrapper.
     */
    TfliteModel() = default;

    /**
     * @brief Destroy the TensorFlow Lite model wrapper.
     */
    ~TfliteModel() override = default;

    /**
     * @brief Get an input tensor wrapper by index.
     *
     * @param  index  Zero-based input tensor index.
     * @return        Tensor wrapper, or an empty pointer when the index is invalid.
     */
    std::shared_ptr<iface::TensorIface> GetInputTensor(size_t index) const override;

    /**
     * @brief Get an output tensor wrapper by index.
     *
     * @param  index  Zero-based output tensor index.
     * @return        Tensor wrapper, or an empty pointer when the index is invalid.
     */
    std::shared_ptr<iface::TensorIface> GetOutputTensor(size_t index) const override;

    /**
     * @brief Get the data type of the first input tensor.
     *
     * @return  First input tensor type, or INVALID before initialization.
     */
    iface::TensorType GetType() const override;

    /**
     * @brief Get the shape of an input tensor.
     *
     * @param  index  Zero-based input tensor index.
     * @return        Tensor shape, or an empty vector when the index is invalid.
     */
    std::vector<size_t> GetInputShape(size_t index) const override;

    /**
     * @brief Get the shape of an output tensor.
     *
     * @param  index  Zero-based output tensor index.
     * @return        Tensor shape, or an empty vector when the index is invalid.
     */
    std::vector<size_t> GetOutputShape(size_t index) const override;

    /**
     * @brief Get the number of model input tensors.
     *
     * @return  Number of input tensors, or zero before interpreter creation.
     */
    size_t GetNumInputs() const override;

    /**
     * @brief Get the number of model output tensors.
     *
     * @return  Number of output tensors, or zero before interpreter creation.
     */
    size_t GetNumOutputs() const override;

    /**
     * @brief Log details for one tensor.
     *
     * @param  tensor  Tensor wrapper to describe.
     */
    void LogTensorInfo(std::shared_ptr<iface::TensorIface> tensor) override;

    /**
     * @brief Log details for all model input and output tensors.
     */
    void LogInterpreterInfo() override;

    /**
     * @brief Initialize the TensorFlow Lite model and interpreter.
     *
     * @param  computeBuffer  Compute buffer tracked by the model interface.
     * @param  nnModel        Optional in-memory TensorFlow Lite flatbuffer.
     * @param  backendData    Optional pointer to TfliteBackendOptions.
     * @return                True when initialization succeeds, otherwise false.
     */
    bool Init(iface::MemoryRegion& computeBuffer,
              iface::MemoryRegion& nnModel,
              const void* backendData) override;

    /**
     * @brief Check whether the model has been initialized.
     *
     * @return  True after successful initialization, otherwise false.
     */
    bool IsInited() const override;

    /**
     * @brief Check whether the model input tensor type is signed.
     *
     * @return  True for signed integer tensor types, otherwise false.
     */
    bool IsDataSigned() const override;

    /**
     * @brief Check whether the flatbuffer contains an Ethos-U custom operator.
     *
     * @return  True when an Ethos-U custom operator is present, otherwise false.
     */
    bool ContainsEthosUOperator() const override;

    /**
     * @brief Invoke the TensorFlow Lite interpreter.
     *
     * @return  True when invocation succeeds, otherwise false.
     */
    bool RunInference() override;

    /**
     * @brief Get the compute buffer tracked by the model interface.
     *
     * @return  Compute buffer memory region.
     */
    const iface::MemoryRegion& GetComputeBuffer() const override;

    /**
     * @brief Get the model buffer tracked by the model interface.
     *
     * @return  Model buffer memory region.
     */
    const iface::MemoryRegion& GetModelBuffer() const override;

private:
    /**
     * @brief Load a TensorFlow Lite flatbuffer from file or memory.
     *
     * @param  nnModel  Optional in-memory TensorFlow Lite flatbuffer.
     * @param  options  Backend options containing the optional model path.
     * @return          True when the model is loaded successfully, otherwise false.
     */
    bool LoadModel(const iface::MemoryRegion& nnModel, const TfliteBackendOptions& options);

    /**
     * @brief Create the TensorFlow Lite interpreter and apply delegates.
     *
     * @param  options  Backend options controlling interpreter creation.
     * @return          True when interpreter creation succeeds, otherwise false.
     */
    bool BuildInterpreter(const TfliteBackendOptions& options);

    /**
     * @brief Wrap interpreter input and output tensors with model interfaces.
     */
    void WrapTensors();

#if defined(ETHOS_U_NPU_ENABLED)
    /**
     * @brief Deleter for TensorFlow Lite external delegate handles.
     */
    struct ExternalDelegateDeleter {
        /**
         * @brief Destroy an external delegate handle.
         *
         * @param  delegate  Delegate handle to destroy.
         */
        void operator()(TfLiteDelegate* delegate) const;
    };

    /**
     * @brief Create and apply configured TensorFlow Lite external delegates.
     *
     * @param  options  Backend options containing delegate settings.
     * @return          True when delegate setup succeeds, otherwise false.
     */
    bool SetupDelegates(const TfliteBackendOptions& options);
    std::unique_ptr<TfLiteDelegate, ExternalDelegateDeleter> m_externalDelegate{};
#endif

    std::unique_ptr<::tflite::FlatBufferModel> m_model{};
    std::unique_ptr<::tflite::OpResolver> m_opResolver{};
    std::unique_ptr<::tflite::Interpreter> m_interpreter{};
    bool m_inited{false};
    iface::MemoryRegion m_computeBuffer{};
    iface::MemoryRegion m_modelBuffer{};
    std::vector<std::shared_ptr<iface::TensorIface>> m_input{};
    std::vector<std::shared_ptr<iface::TensorIface>> m_output{};
    iface::TensorType m_type{iface::TensorType::INVALID};
};

} // namespace arm::app::fwk::tflite

#endif // TFLITE_MODEL_HPP
