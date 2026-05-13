#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

#!/bin/bash

# Install individual packages
echo "Installing required packages for the Audiogen module..."

# Stable audio tools
pip install "stable_audio_tools==0.0.19"

# LiteRT Torch
pip install "litert-torch==0.9.0"

# stable_audio_tools has a dependency on numpy 1.26.4, we need this version, otherwise it fails.
pip install --no-deps "numpy==1.26.4"

echo "Finished installing required packages for AudioGen submodules conversion."
echo "To start converting the Conditioners, DiT and Autoencoder modules conversion, use the following command:"
echo "python ./scripts/export_sao.py"
