#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

import argparse
import json
import logging
import os

import torch

from model import (get_dit_module, load_model,
                    get_autoencoder_decoder_module,
                    get_autoencoder_decoder_example_input,
                    get_autoencoder_encoder_module,
                    get_autoencoder_encoder_example_input,
                    get_conditioners_module,
                    get_conditioners_example_input,
                    get_dit_example_input_mapping)

from stable_audio_tools.models.utils import remove_weight_norm_from_model

import litert_torch

from litert_torch.generative.quantize import quant_recipe, quant_recipe_utils
from litert_torch.quantize import quant_config

logging.basicConfig(level=logging.INFO)

os.environ["CUDA_VISIBLE_DEVICES"] = ""

T5_SEQ_LENGTH = 128

def export_conditioners(model, output_path) -> None:

    with torch.no_grad():
        conditioners_model = get_conditioners_module(model)
        conditioners_model = conditioners_model.eval().requires_grad_(False)
        conditioners_example_input = get_conditioners_example_input(seq_length=T5_SEQ_LENGTH, seconds_total=10.0)

        edge_model = litert_torch.convert(
            conditioners_model, sample_args=conditioners_example_input, sample_kwargs=None
        )

        edge_model.export(os.path.join(output_path, "conditioners_float32.tflite"))
        logging.info("Conditioners model has been saved to %s", os.path.abspath(os.path.join(output_path, "conditioners_float32.tflite")))

def export_dit(model, output_path, dtype = torch.float) -> None:

    logging.info("Starting DiT Model conversion to LiteRT format...\n")

    with torch.no_grad():
        dit_model = get_dit_module(model=model)
        dit_model = dit_model.to(dtype).eval().requires_grad_(False)
        dit_model_example_input = get_dit_example_input_mapping(dtype)

        # Create the dynamic weights int8 quantization config
        quant_config_audiogen_int8 = quant_config.QuantConfig(
            generative_recipe=quant_recipe.GenerativeQuantRecipe(
                default=quant_recipe_utils.create_layer_quant_dynamic(),
            )
        )

        # Workaround for some issue in LiteRT that occurs at runtime
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

        edge_model.export(os.path.join(output_path, "dit_model.tflite"))
        logging.info("DiT model has been saved to %s", os.path.abspath(os.path.join(output_path, "dit_model.tflite")))

def export_autoencoder(model, output_path, dtype = torch.float) -> None:

    logging.info("Starting AutoEncoder Decoder conversion...\n")

    with torch.no_grad():
        autoencoder_decoder_example_input = get_autoencoder_decoder_example_input(dtype=dtype)
        # model.pretransform.model_half=False
        model = model.to(dtype).eval().requires_grad_(False)

        autoencoder_decoder = get_autoencoder_decoder_module(model)
        autoencoder_decoder = autoencoder_decoder.to(dtype).eval().requires_grad_(False)

        # Export the model to LiteRT format
        edge_model = litert_torch.convert(
            autoencoder_decoder, sample_args=autoencoder_decoder_example_input,
        )
        edge_model.export(os.path.join(output_path, "autoencoder_model.tflite"))
        logging.info("AutoEncoder Decoder model has been saved to %s", os.path.abspath(os.path.join(output_path, "autoencoder_model.tflite")))

def export_autoencoder_encoder(model, output_path, dtype = torch.float) -> None:

    logging.info("Starting AutoEncoder Encoder conversion...\n")

    with torch.no_grad():
        autoencoder_encoder_example_input = get_autoencoder_encoder_example_input(dtype=dtype)
        # model.pretransform.model_half=False
        model = model.to(dtype).eval().requires_grad_(False)

        autoencoder_encoder = get_autoencoder_encoder_module(model)
        autoencoder_encoder = autoencoder_encoder.to(dtype).eval().requires_grad_(False)

        # Export the model to LiteRT format
        edge_model = litert_torch.convert(
            autoencoder_encoder, sample_args=autoencoder_encoder_example_input,
        )
        edge_model.export(os.path.join(output_path, "autoencoder_encoder_model.tflite"))
        logging.info("AutoEncoder Encoder model has been saved to %s", os.path.abspath(os.path.join(output_path, "autoencoder_encoder_model.tflite")))

def export(args) -> None:

    torch.manual_seed(0)
    device = torch.device("cpu")

    # Load the model configuration
    logging.info("Loading the AudioGen Checkpoint...")
    with open(args.model_config, encoding="utf-8") as f:
        model_config = json.load(f)

    # Load the model
    model, model_config = load_model(
        model_config = model_config,
        model_ckpt_path = args.ckpt_path,
        pretrained_name=None,
        device=device,
    )
    logging.info("Model is loaded...")

    # --------- Conditioners Model ---------
    export_conditioners(model, args.output_path)

    # --------- DiT Model ----------------
    export_dit(model, args.output_path)

    # --------- AutoEncoder Model ---------

    # Removing weight norm from the model as it is causing issues during export
    remove_weight_norm_from_model(model.pretransform)

    export_autoencoder(model, args.output_path)
    export_autoencoder_encoder(model, args.output_path)

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-m",
        "--model_config",
        type=str,
        help="Path to the model configuration file.",
        required=True,
    )
    parser.add_argument(
        "--ckpt_path",
        type=str,
        help="Path to the model checkpoint file.",
        required=True,
    )

    parser.add_argument(
        "--output_path",
        type=str,
        help="Path to the output directory for the exported models.",
        default=".",
        required=False,
    )

    export(parser.parse_args())

if __name__ == "__main__":
    main()