
# Requirements
In order to run the training and quantization scripts, you would need to:
1) Install the [sooftware conformer model](https://github.com/sooftware/conformer) as a pip package.
2) Download the [LibriSpeech dataset from torchaudio](https://docs.pytorch.org/audio/stable/generated/torchaudio.datasets.LIBRISPEECH.html) and tokenizer from the `training` folder.
3) For the post-training quantization, you need to install ExecuTorch from source. We recommend you to install ExecuTorch from a python 3.10 virtual environmental variable.
Clone the [ExecuTorch repository](https://github.com/pytorch/executorch/), checkout the `release/1.0` branch and run `./install_executorch.sh` from the root folder. You also need to install the
Ethos-U backend dependencies within ExecuTorch, you can do that by running `./examples/arm/setup.sh --i-agree-to-the-contained-eula`.
You can find detailed instructions about installing ExecuTorch from source [in the official documentation](https://docs.pytorch.org/executorch/stable/using-executorch-building-from-source.html#install-executorch-pip-package-from-source). The
detailed instructions for setting up the Arm backend are in the [examples/arm folder](https://github.com/pytorch/executorch/tree/main/examples/arm#example-workflow). The key commands are:
```
$ git clone git@github.com:pytorch/executorch.git
$ git checkout git release/1.0
$ git submodule sync && git submodule update --init --recursive
$ ./install_executorch.sh
$ ./examples/arm/setup.sh --i-agree-to-the-contained-eula
```

## Torchcodec
We use `torchaudio` for the pre-processing of the LibriSpeech dataset. Since [August 2025](https://github.com/pytorch/audio/commit/93f582ca5001132bfcdb115f476b73ae60e6ef8a), torchaudio requires torchcodec.
You need to install `torchcodec` in order to be able to load audio samples with torchaudio. When you install ExecuTorch from the release/1.0 branch, you will get torchaudio 2.9.0 :
```
$ pip freeze | grep torch
torch==2.9.0
torchaudio==2.9.0
torchvision==0.24.0
....
```
Manually install the torchcodec package. Empirically, we've observed that torchcodec version 0.7.0.dev20250915 works
with torchaudio 2.9.0.
```
$ pip install --pre --no-deps --index-url https://download.pytorch.org/whl/nightly/cpu \
  "torchcodec==0.7.0.dev20250915"
```
As per the [torchcodec documentation](https://github.com/pytorch/torchcodec?tab=readme-ov-file#installing-torchcodec), you need to ensure you have a version of `ffmpeg` smaller than 8.
On a Mac OS, you also need to export the `DYLD_FALLBACK_LIBRARY_PATH` environment variable to the location of the torchcodec binaries.
```
export DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/opt/ffmpeg@7/lib:/opt/homebrew/lib"
```

You can now use the latest torchaudio and load audio recordings with torchcodec.

# Quantization 

The `ptq_evaluate_conformer_10M.py` script provides a way to quantize a Conformer speech recognition network, evaluate its accuracy on the LibriSpeech dataset and generate an ExecuTorch pte for the Ethos-U NPU.
We assume you have obtained a trained checkpoint from the Training section. Run the `ptq_evaluate_10M_model.py` script to obtain a pte file that will be deployed on device in the following way:
 ```
 $ python ptq_evaluate_conformer_10M.py --root <path to the LibriSpeech dataset> --dataset <dataset, usually test-clean> --checkpoint <path to checkpoint with trained weights> --sp-model <path to the tokenizer>
 ```

We obtain ~8% Word Error Rate when evaluating the quantized model on the test-clean dataset.