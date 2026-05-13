<!--
    SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Building and Running the Audio Generation Application on Arm® CPUs with the Stable Audio Open Small Model

## Goal
This guide will show you how to convert the Stable Audio Open Small Model to LiteRT format to run on Arm® CPUs with the LiteRT runtime.

### Converting the Stable Audio Open Small Model to LiteRT format
The Stable Audio Open Small Model is made of three submodules:
- Conditioners (Text conditioner and number conditioners)
- Diffusion Transformer (DiT)
- AutoEncoder.

You will explore how to use LiteRT torch for those models.

__PyTorch → LiteRT__ using the [LiteRT Torch](https://github.com/google-ai-edge/litert-torch) tool. This tool aims to simplify the conversion and the quantization of torch models to LiteRT, for easy deployment on edge devices.

### Create a virtual environment and install dependencies.

#### Step 1
In the `/audiogen` folder, create and activate a virtual environment (it is recommended to use Python 3.10 for compatibility with the specified packages)
```bash
python3.10 -m venv .venv
source .venv/bin/activate
```
#### Step 2
Install the required dependencies. These dependencies are specified in [`install_requirements.sh`](../install_requirements.sh). You can install them by using a bash script (option A) or manually using pip install (option B).

<strong> Option A</strong>
```bash
# Option A (with .venv activated)
bash install_requirements.sh
```

<strong> Option B</strong>
```bash
# Option B (with .venv activated)

# Stable audio tools
pip install "stable_audio_tools==0.0.19"

# Install LiteRT Torch
pip install "litert-torch==0.9.0"

# Install numpy with this version
pip install --no-deps "numpy==1.26.4"

```

###  Exporting the models
To convert the models, we use the [Generative API](https://github.com/google-ai-edge/litert-torch/tree/main/litert_torch/generative) provided in by the `litert_torch` tools. This API supports exporting a PyTorch model directly to LiteRT following three mains steps; model re-authoring, quantization, and finally conversion.

Here is a code snippet illustrating how the API works in practice.
```python
import litert_torch
from litert_torch.quantize import quant_config
from litert_torch.generative.quantize import quant_recipe, quant_recipe_utils


# Specify the quantization format
quant_config_int8 = quant_config.QuantConfig(
        generative_recipe=quant_recipe.GenerativeQuantRecipe(
        default=quant_recipe_utils.create_layer_quant_dynamic(),
    )
)
# Initiate the conversion
edge_model = litert_torch.convert(
    model, example_inputs, quant_config=quant_config_int8
)
```
Notes on the arguments for `litert_torch.convert()`:
- __model__: The PyTorch model to be converted. This should be the pre-trained model loaded from the `.config` and `.ckpt` files, and set to evaluation mode (model.eval()).
- __example_inputs__: A tuple of torch.Tensor objects. These are dummy input tensors that match the expected shape and type of your model's forward pass arguments. For models with multiple inputs, provide them as a tuple in the correct order.

To convert the models, run the [`export_sao.py`](./export_sao.py) script using the following command (ensure your .venv is still active):

```bash
python3 ./scripts/export_sao.py --model_config "$WORKSPACE/model_config.json" --ckpt_path "$WORKSPACE/model.ckpt"
```

The three LiteRT format models will be required to run the audiogen application on Android™ device.

You can now follow the instructions located in the [`app/`](../app/README.md) directory to build the audio generation application.