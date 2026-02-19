#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Disable GPU to avoid any issues during export
import os
os.environ["CUDA_VISIBLE_DEVICES"] = ""

import argparse
import json
import logging

import litert_torch
import torch

from einops import rearrange

from litert_torch.generative.quantize import quant_recipe, quant_recipe_utils
from litert_torch.quantize import quant_config
from utils_load_model import load_model

import stable_audio_tools

torch.manual_seed(0)
DEVICE = torch.device("cpu")

logging.basicConfig(level=logging.INFO)

## ----------------- Utility Functions DiT -------------------
def get_dit_example_input_mapping(dtype=torch.float):
    """Provide example input tensors for the DiT model as a dictionary.
    Args:
        dtype (torch.dtype): The data type for the input tensors.
    Returns:
        dict: A dictionary containing the example input tensors for the DiT model.
        x (torch.Tensor): The input tensor for the DiT model.
        t (torch.Tensor): The time tensor for the DiT model.
        cross_attn_cond (torch.Tensor): The cross attention conditioning tensor for the DiT model. Output of the Conditioner T5 Encoder.
        global_cond (torch.Tensor): The global conditioning tensor for the DiT model. Output of the Conditioner Number Encoder.
    """
    return {
        "x": torch.rand(size=(1, 64, 256), dtype=dtype, requires_grad=False),  # x
        "t": torch.tensor([0.154], dtype=dtype, requires_grad=False),  # t
        "cross_attn_cond": torch.rand(
            size=(1, 65, 768), dtype=dtype, requires_grad=False
        ),  # cross_attn_cond
        "global_cond": torch.rand(size=(1, 768), dtype=dtype, requires_grad=False),  # global_cond
    }


## ----------------- Utility Functions AutoEncoder -------------------
def get_autoencoder_decoder_module(model):
    """Get the AutoEncoder module from the AudioGen model."""
    return AutoEncoderDecoderModule(model.pretransform)

def get_autoencoder_encoder_module(model):
    """Get the AutoEncoder module from the AudioGen model."""
    return AutoEncoderEncoderModule(model.pretransform)

def get_autoencoder_decoder_example_input(dtype=torch.float):
    """Get example input for the AutoEncoder module."""
    return (torch.rand((1, 64, 256), dtype=dtype),)

def get_autoencoder_encoder_example_input(dtype=torch.float):
    """Get example input for the AutoEncoder module."""
    return (torch.rand((1, 2, 524288), dtype=dtype),)


class AutoEncoderDecoderModule(torch.nn.Module):
    """Wrap the AutoEncoder Module. Takes the AutoEncoder and returns the audio.
    Args:
        autoencoder (torch.nn.Module): The AutoEncoder module.
    Returns:
        audio (torch.Tensor): The decoded audio tensor.
    """

    def __init__(self, autoencoder):
        super(AutoEncoderDecoderModule, self).__init__()
        self.autoencoder = autoencoder

        # Use float
        self.autoencoder = (
            self.autoencoder.to(dtype=torch.float).eval().requires_grad_(False)
        )

    def forward(self, sampled: torch.Tensor):
        dtype = torch.float
        sampled_uncompressed = self.autoencoder.decode(sampled.to(dtype))

        audio = rearrange(sampled_uncompressed, "b d n -> d (b n)")

        return audio

def vae_sample_updated(mean, scale):
    stdev = torch.nn.functional.softplus(scale) + 1e-4
    var = stdev * stdev
    logvar = torch.log(var)

    # "randn_like" was causing failures while exporting the model:
    # latents = torch.randn_like(mean) * stdev + mean
    rand = torch.randn(mean.size())
    latents = rand * stdev + mean

    kl = (mean * mean + var - logvar - 1).sum(1).mean()

    return latents, kl

class AutoEncoderEncoderModule(torch.nn.Module):
    """Wrap the AutoEncoder Module. Takes the AutoEncoder and returns the audio.
    Args:
        autoencoder (torch.nn.Module): The AutoEncoder module.
    Returns:
        audio (torch.Tensor): The decoded audio tensor.
    """

    def __init__(self, autoencoder):
        super(AutoEncoderEncoderModule, self).__init__()
        self.autoencoder = autoencoder

        # Use float
        self.autoencoder = (
            self.autoencoder.to(dtype=torch.float).eval().requires_grad_(False)
        )

        stable_audio_tools.models.bottleneck.vae_sample = vae_sample_updated

    def forward(self, sampled: torch.Tensor):
        dtype = torch.float
        sample_compressed = self.autoencoder.encode(sampled.to(dtype))

        return sample_compressed

## ----------------- Exporting DiT and AutoEncoder to LiteRT format -------------------
def export_audiogen(args) -> None:
    """Export the Dit and AutoEncoder of the AudioGen model to LiteRT format."""

    model_config = None
    dtype = torch.float32

    # Load the model Configuration
    logging.info("Loading the AudioGen Checkpoint...")
    with open(args.model_config, encoding="utf-8") as f:
        model_config = json.load(f)
    model, model_config = load_model(
        model_config,
        args.ckpt_path,
        pretrained_name=None,
        device=DEVICE,
    )

    logging.info(
        "Exporting the model, ckpt: %s, with config %s.",
        args.ckpt_path,
        args.model_config,
    )

    # Create the dynamic weights int8 quantization config
    quant_config_audiogen_int8 = quant_config.QuantConfig(
        generative_recipe=quant_recipe.GenerativeQuantRecipe(
            default=quant_recipe_utils.create_layer_quant_dynamic(),
        )
    )

    ## --------- DiT Model ---------
    # Load the diffusion transformer model (DiT)
    logging.info("Starting DiT Model conversion to LiteRT format...\n")
    dit_model = model.model
    dit_model = dit_model.to(dtype).eval().requires_grad_(False)
    dit_model_example_input = get_dit_example_input_mapping(dtype)
    logging.info("Exporting the DiT model...")

    # # Workaround for some issue in LiteRT that occurs at runtime
    rotary_pos_emb_res = (
        dit_model.model.transformer.rotary_pos_emb.forward_from_seq_len(257)
    )
    def rotary_emb_const(_):
        return rotary_pos_emb_res
    dit_model.model.transformer.rotary_pos_emb.forward_from_seq_len = rotary_emb_const

    # Export the DiT to LiteRT format
    edge_model = litert_torch.convert(
        dit_model, sample_args=None, sample_kwargs=dit_model_example_input, quant_config=quant_config_audiogen_int8
    )
    edge_model.export("./dit_model.tflite")
    logging.info("DiT model has been saved to %s/dit_model.tflite")

    ## --------- AutoEncoder Decoder Model ---------
    # Load the Encoder part of the AutoEncoder
    logging.info("Starting AutoEncoder Decoder Model conversion to LiteRT format...\n")
    autoencoder_decoder = get_autoencoder_decoder_module(model)
    autoencoder_decoder = autoencoder_decoder.to(dtype).eval().requires_grad_(False)
    autoencoder_decoder_example_input = get_autoencoder_decoder_example_input(dtype)

    # Export the Encoder part of the AutoEncoder to LiteRT format
    edge_model = litert_torch.convert(
        autoencoder_decoder,
        autoencoder_decoder_example_input,
    )
    edge_model.export("./autoencoder_model.tflite")
    logging.info(
        "AutoEncoder model has been saved to %s/autoencoder_model.tflite",
    )

    ## --------- AutoEncoder Encoder Model ---------
    # Load the Encoder part of the AutoEncoder
    logging.info("Starting AutoEncoder Encoder Model conversion to LiteRT format...\n")
    autoencoder_encoder = get_autoencoder_encoder_module(model)
    autoencoder_encoder = autoencoder_encoder.to(dtype).eval().requires_grad_(False)
    autoencoder_encoder_example_input = get_autoencoder_encoder_example_input(dtype)

    # Export the AutoEncoder to LiteRT format
    edge_model = litert_torch.convert(
        autoencoder_encoder,
        autoencoder_encoder_example_input,
    )
    edge_model.export("./autoencoder_encoder_model.tflite")
    logging.info(
        "AutoEncoder model has been saved to %s/autoencoder_encoder_model.tflite",
    )


def main():
    """Main function to export the AudioGen model to LiteRT format."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-m",
        "--model_config",
        type=str,
        help="Path to model config",
        required=True
    )
    parser.add_argument(
        "-p",
        "--ckpt_path",
        type=str,
        help="Path to model checkpoint",
        required=True
    )
    export_audiogen(parser.parse_args())


if __name__ == "__main__":
    main()
