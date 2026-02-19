#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

#!/bin/bash

# Install individual packages
echo "Installing required packages for the Audiogen module..."

# LiteRT torch
pip install litert-torch==0.8.0 \
    "ai-edge-litert==2.1.2" \
    "ai-edge-quantizer==0.4.2"

# Stable audio tools
pip install "stable_audio_tools==0.0.19"


# Working out dependency issues, this combination of packages has been tested on different systems (Linux and MacOS).
pip install --no-deps "torch==2.9.0" \
                      "torchaudio==2.9.0" \
                      "torchvision==0.24.0" \
                      "protobuf==5.29.6" \
                      "numpy==1.26.4" \

# Packages to convert via onnx
pip install --no-deps "onnx==1.18.0" \
                      "onnxsim==0.4.36" \
                      "onnx-ir==0.1.16" \
                      "onnx2tf==1.27.10" \
                      "onnxscript==0.6.2" \
                      "tensorflow==2.19.0" \
                      "tf_keras==2.19.0" \
                      "onnx-graphsurgeon==0.5.8" \
                      "sng4onnx==1.0.4"

echo "Finished installing required packages for AudioGen submodules conversion."
echo "To start converting the Conditioners, DiT and Autoencoder modules conversion, use the following command:"
echo "python ./scripts/export_{MODEL-T0-CONVERT}.py"
