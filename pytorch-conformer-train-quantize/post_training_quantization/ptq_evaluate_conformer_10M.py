#  Copyright (c) 2025 Arm Limited. All rights reserved.
#  SPDX-License-Identifier: Apache-2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import argparse
import random

import sentencepiece as spm
import torch

# Clone https://github.com/sooftware/conformer/tree/main/conformer and pip install the model
from conformer import Conformer
from executorch.backends.arm.ethosu import EthosUPartitioner, EthosUCompileSpec

from executorch.backends.arm.quantizer import (
    EthosUQuantizer,
    get_symmetric_quantization_config,
)
from executorch.exir import (
    EdgeCompileConfig,
    ExecutorchBackendConfig,
    to_edge_transform_and_lower,
)
from executorch.extension.export_util.utils import save_pte_program
from torch.export import export

from torch.nn.utils.rnn import pad_sequence

from torch.utils.data import DataLoader, Subset

from torchao.quantization.pt2e.quantize_pt2e import convert_pt2e, prepare_pt2e

from torchaudio.datasets import LIBRISPEECH
from torchaudio.transforms import MelSpectrogram
from torcheval.metrics import WordErrorRate
from torcheval.metrics.functional import word_error_rate

CHUNK_SIZE = 1500
BLANK = 0
device = torch.device("cpu")


# text_to_int and int_to_text - same as how we do it during training
def text_to_int(text: str, sp) -> torch.Tensor:
    ids = sp.encode(text.lower(), out_type=int)
    # shift by +1 so blank stays at 0
    ids = [i + 1 for i in ids]
    return torch.tensor(ids, dtype=torch.long)


def int_to_text(token_ids: list[int], sp) -> str:
    # Collapse repeats & remove blank (0)
    pieces, prev = [], None
    for t in token_ids:
        if t == BLANK:
            prev = None
            continue
        if t == prev:
            continue
        raw = t - 1
        pieces.append(raw)
        prev = t
    return sp.decode(pieces)


# collate_fn function as per the pre-processing done in the training loop
# The difference compared to training loop is that here, because we want fixed input shape (1,CHUNK_SIZE,80) to the NN,
# in case the audio recording has more samples than CHUNK_SIZE, we split the audio recording into multiple recordings,
# all of shape (1,CHUNK_SIZE_80) and we are padding the end of the spectrogram
def make_simple_collate_fn(sp, mel_transform):
    def collate_fn(batch):
        feats, feat_lens, tgts, tgt_lens, txts = [], [], [], [], []
        for waveform, _, transcript, *_ in batch:
            spec = mel_transform(waveform).squeeze(0).transpose(0, 1)
            spec = spec.add_(1e-6).log()
            T = spec.shape[0]  # number of timesteps in the original spectrogram
            pad_len = (CHUNK_SIZE - (T % CHUNK_SIZE)) % CHUNK_SIZE
            if pad_len > 0:
                pad = -20 * torch.ones(pad_len, spec.size(1), dtype=spec.dtype)
                spec = torch.cat((spec, pad), dim=0)

            num_samples = spec.shape[0] // CHUNK_SIZE
            if num_samples > 1:
                print(
                    f"[CHUNKED] Transcript split into {num_samples} chunks:\nOriginal transcript: {transcript.lower()}"
                )

            chunked = spec.view(num_samples, CHUNK_SIZE, spec.shape[1])
            ids = [i + 1 for i in sp.encode(transcript.lower(), out_type=int)]
            token_ids = torch.tensor(ids, dtype=torch.long)
            tgts.append(token_ids)
            tgt_lens.append(len(token_ids))
            txts.append(transcript.lower())

            for j in range(num_samples):
                feats.append(chunked[j])
                feat_lens.append([CHUNK_SIZE])
                tgts.append(token_ids)
                tgt_lens.append(len(token_ids))
                txts.append(transcript.lower())

        feats = torch.stack(feats)
        feat_lens = torch.tensor(feat_lens)
        tgts = pad_sequence(tgts, batch_first=True)
        tgt_lens = torch.tensor(tgt_lens)
        return feats, feat_lens, tgts, tgt_lens, txts

    return collate_fn


# decoding as per the training loop
def greedy_decode(logits: torch.Tensor, lens: torch.Tensor, sp) -> list[str]:
    best = logits.argmax(-1)  # (B, max_T)
    transcripts = []
    for i, seq in enumerate(best):
        valid = seq[: int(lens[i])]  # trim padding
        transcripts.append(int_to_text(valid.tolist(), sp))
    return transcripts


def evaluate_model(model, dataloader, sp):
    print("Evaluating model in PyTorch eager mode")
    wer_metric = WordErrorRate()
    i = 0
    with torch.no_grad():
        for feats, feat_lens, _, _, refs in dataloader:
            i = i + 1
            chunk_hyps = []
            # Iterate over each sample(we'll have more than one sample in case we've had to split it)
            for j in range(feats.shape[0]):
                chunk = feats[j].unsqueeze(0)  # shape: [1, CHUNK_SIZE, 80]
                chunk_len = feat_lens[j].unsqueeze(0)  # shape: [1]
                logits, logit_lens = model(chunk, chunk_len)
                hyp = greedy_decode(logits.cpu(), logit_lens.cpu(), sp)[0]
                chunk_hyps.append(hyp)

            merged_hyp = " ".join(chunk_hyps).strip()

            # The reference should be the original text (not duplicated N times in case the recording is split)
            ref = refs[0].strip()

            print(f"reference = {ref}")
            print(f"predicted = {merged_hyp}")
            wer_metric.update([ref], [merged_hyp])
            sample_wer = word_error_rate([ref], [merged_hyp])
            print(
                f"[{i}] Sample WER = {sample_wer:.2f} Aggregate WER = {wer_metric.compute():.2f}\n"
            )
    return wer_metric.compute()


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--root", type=str, required=True, help="Path to the root of LibriSpeech"
    )
    parser.add_argument(
        "--checkpoint",
        type=str,
        required=True,
        help="Path to the conformer pth file containing the state dictionary of the model",
    )
    parser.add_argument(
        "--sp-model",
        type=str,
        default="saved_tokenizer_nemo_stt_en_conformer_ctc_small.model",
        help="Path to the SentencePiece *.model(aka the tokenizer) used at training time",
    )
    parser.add_argument(
        "--dataset",
        type=str,
        required=True,
        help="Dataset type, e.g. test-clean, test-other",
    )
    parser.add_argument(
        "--ethos-u-memory-mode",
        type=str,
        required=False,
        default="Dedicated_Sram_384KB",
        help="Ethos-U memory mode defined in the vela.ini file, e.g. Sram_Only, Shared_Sram, Dedicated_Sram_384KB"
    )
    parser.add_argument(
        "--ethos-u-variant",
        type=str,
        required=False,
        default="ethos-u85-1024",
        help="Ethos-U variant, e.g. ethos-u85-128, ethos-u85-256"
    )
    parser.add_argument(
        "--ethos-u-system-config",
        type=str,
        required=False,
        default="Ethos_U85_SYS_DRAM_Mid",
        help="Ethos-U system configuration defined in the vela.ini file, e.g. Ethos_U85_SYS_Flash_High or Ethos_U85_SYS_DRAM_Mid"
    )


    args = parser.parse_args()

    # Load the tokenizer
    # -------- SentencePiece ----------
    sp = spm.SentencePieceProcessor()
    sp.load(args.sp_model)

    pth = args.checkpoint

    # Pre-processing as per the Conformer paper & as how the NN was trained
    mel_transform = MelSpectrogram(
        sample_rate=16000, n_fft=512, win_length=512, hop_length=160, n_mels=80, center=False
    )

    dataset = LIBRISPEECH(root=args.root, url=args.dataset, download=True)

    # Pick 100 random indexes for calibration and 200 random indexes for evaluation.
    # For more accurate int8 model, you can calibrate over more samples.
    random_indexes_calib = random.sample(range(len(dataset)), 100)
    random_indexes_valid = random.sample(range(len(dataset)), 200)
    calibration_set = Subset(dataset, random_indexes_calib)
    test_set = Subset(dataset, random_indexes_valid)

    print("Single sample load to ensure you can load audio samples with torchaudio. In case of problems, follow the torchcodec and ffmpeg instructions in the README.")
    _ = calibration_set[0]

    collate_fn = make_simple_collate_fn(sp, mel_transform)
    calibration_loader = DataLoader(
        calibration_set, batch_size=1, shuffle=False, collate_fn=collate_fn
    )
    test_loader = DataLoader(
        test_set,
        batch_size=1,
        shuffle=False,
        collate_fn=collate_fn,
    )

    vocab_size = sp.get_piece_size() + 1  # +1 needed for the CTC Loss function
    # Conformer model with the same hyper parameters as how we have trained it.
    model = Conformer(
        num_classes=vocab_size,
        input_dim=80,
        encoder_dim=144,
        num_encoder_layers=16,
        num_attention_heads=4,
        feed_forward_expansion_factor=4,
        conv_expansion_factor=2,
        input_dropout_p=0.1,
        feed_forward_dropout_p=0.1,
        attention_dropout_p=0.1,
        conv_dropout_p=0.1,
        conv_kernel_size=31,
        half_step_residual=True,
    ).to(device)

    # Load the checkpoint, state dict of the trained FP32 model and put the model in eval mode
    checkpoint = torch.load(pth, map_location=device, weights_only=True)
    model.load_state_dict(checkpoint["model"])
    model.eval()

    print("Exporting the PyTorch FP32 model")
    example_inputs = (
        torch.rand(1, CHUNK_SIZE, 80),
        torch.tensor([CHUNK_SIZE], dtype=torch.int32),
    )
    exported_program = export(model, example_inputs, strict=True)
    graph_module = exported_program.module(check_guards=False)

    # Get Ethos-U compile specific information
    npu_variant = args.ethos_u_variant
    memory_mode = args.ethos_u_memory_mode
    system_config = args.ethos_u_system_config

    compile_spec = EthosUCompileSpec(
            target=npu_variant,
            system_config=system_config,
            memory_mode=memory_mode,
            extra_flags=["--output-format=raw", "--debug-force-regor"],
        )
    quantizer = EthosUQuantizer(compile_spec)
    config = get_symmetric_quantization_config(is_per_channel=True)
    quantizer.set_global(config)

    quantized_graph_module = prepare_pt2e(graph_module, quantizer)

    print("Calibrating...")
    for feats, feat_lens, *_ in calibration_loader:
        feats, feat_lens, *_ = next(
            iter(calibration_loader)
        )  # only take the first batch in case we've had to split the recording into multiple samples
        feats = feats[:1] # (1, CHUNK_SIZE, 80)
        quantized_graph_module(feats, feat_lens)

    quantized_graph_module = convert_pt2e(quantized_graph_module)

    fp32_wer = evaluate_model(model, test_loader, sp)
    int8_wer = evaluate_model(quantized_graph_module, test_loader, sp)
    print(f"FP32 WER PyTorch eager mode: {fp32_wer:.2%}")
    print(f"INT8 WER PyTorch eager mode: {int8_wer:.2%}")

    # Create partitioner from compile spec
    partitioner = EthosUPartitioner(compile_spec)

    quantized_exported_program = export(
        quantized_graph_module, example_inputs, strict=True
    )
    print(
        "Calling to_edge_transform_and_lower - lowering to TOSA and compiling for the Ethos-U hardware"
    )
    # Lower the exported program to the Ethos-U backend
    edge_program_manager = to_edge_transform_and_lower(
        quantized_exported_program,
        partitioner=[partitioner],
        compile_config=EdgeCompileConfig(
            _check_ir_validity=False,
        ),
    )
    executorch_program_manager = edge_program_manager.to_executorch(
        config=ExecutorchBackendConfig(extract_delegate_segments=False)
    )
    save_pte_program(
        executorch_program_manager, f"conformer_quantized_{npu_variant}_{memory_mode}.pte"
    )


if __name__ == "__main__":
    main()
