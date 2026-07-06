# Deploying

Deployment means attaching a packaged FAT MMC image to the Arm® Corstone™-1000
reference package with an Arm® Cortex®-A320 processor FVP, booting the Yocto
Project™ image, and running examples from the mounted MMC filesystem.

## Prerequisites

You need:

* A Yocto Project workspace for Corstone-1000 with `meta-arm`, `meta-ethos`,
  and a built Corstone-1000 package/full BSP.
* `kas==4.4` available in the environment used to launch the workspace. The project
  resource setup environment installs `kas==4.4` from `scripts/py/requirements.txt`.
* A packaged MMC image. See [PACKAGING.md](PACKAGING.md).

The model setup, project build, and MMC packaging steps do not require this
Yocto Project workspace. The workspace is only needed when running the image.

Create the workspace by following the latest Corstone-1000 build guide:
https://corstone1000.docs.arm.com/en/latest/user-guide.html#build

The FVP helper defaults can be supplied with:

```text
CORSTONE1000_WORK_DIR=/path/to/corstone-1000-workspace
KAS=/path/to/kas
```

Alternatively, pass `--work-dir`, `--fvp-config`, and `--kas` explicitly. If
`--kas` is not provided, the helper checks `$KAS`, then `downloads/env/bin/kas`,
then `PATH`. If `--fvp-config` is not provided, the helper uses the built flash
firmware `.fvpconf` under the workspace deploy directory.

The helper uses the workspace's Corstone-1000 with Cortex-A320 FVP executable by
default. To override that path, pass `--fvp` or set `FVP_CORSTONE1000`.

## Launch the FVP

From the project directory:

```sh
scripts/py/launch_fvp.py \
  --work-dir /path/to/corstone-1000-workspace \
  --mmc-image cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img
```

To inspect the generated `kas shell` command without running it:

```sh
scripts/py/launch_fvp.py \
  --mmc-image cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img \
  --dry-run
```

The helper launches `runfvp` with tmux terminals and attaches the MMC image via:

```text
host.ethosu.num_macs=2048
board.msd_mmc.p_mmc_file=<image>
```

By default, each FVP console opens in a separate tmux window. To keep the Normal
World, Secure World, and Secure Enclave consoles visible together, request the
pane layout. If the helper is not already running inside tmux, it starts a tmux
session automatically:

```sh
scripts/py/launch_fvp.py \
  --work-dir /path/to/corstone-1000-workspace \
  --mmc-image cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img \
  --terminal-layout tmux-panes
```

It also enables user networking and maps host port `2222` to target SSH port
`22` when the target image provides SSH. The helper does not include a Dropbear
kas fragment.

## Manual FVP Launch

Developers can run the same flow manually from a built Yocto Project workspace
for Corstone-1000 by using `scripts/py/launch_fvp.py`. The helper generates the
temporary top-level kas config, exports `KAS_WORK_DIR` and `FVP_CORSTONE1000`
for the `kas shell` environment, and passes the MMC override to `runfvp`.
Use `--dry-run` to inspect the generated command:

```sh
scripts/py/launch_fvp.py \
  --work-dir /path/to/corstone-1000-workspace \
  --mmc-image cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img \
  --terminal-layout tmux-panes \
  --dry-run
```

The generated command includes the same `meta-arm` kas fragments as the old
manual flow without passing them as a mixed-repository colon list.

## Running FVP CTests Locally

The Corstone-1000 CTests launch one fresh FVP instance per test and run the
packaged test script from the MMC image. They need the same built Yocto
Project workspace or packaged FVP root as a manual FVP launch. They also use the
project Python virtual environment created by `setup_model_resources.sh`; this
environment provides the Python FVP CTest runner dependencies from
`scripts/py/requirements.txt`.

Each FVP CTest uses a Python runner to launch the FVP, discover terminal ports,
drive the target shell, and collect logs. The runner uses `telnetlib3` to read
the FVP terminal output.

CTest output prints the marked application section from the normal-world
console first. The same section is written to `Testing/FVP/<test>/logs/application.log`.
Boot, shell, FVP, secure-world, and secure-enclave output are kept in separate
files under that `logs` directory. `Testing/FVP/<test>/combined.log` preserves
all sections for deeper debugging.

For a Yocto Project workspace, follow the latest Corstone-1000 build guide:
https://corstone1000.docs.arm.com/en/latest/user-guide.html#build

Create and populate the project Python virtual environment before configuring
or running FVP CTests:

```sh
./setup_model_resources.sh --parallel 8
```

For a local Yocto Project workspace, set:

```sh
export CORSTONE_1000_FVP_ROOT=/path/to/corstone-1000-workspace
```

`CORSTONE1000_WORK_DIR` is also accepted for consistency with the manual
launcher.
The workspace must contain:

```text
meta-arm/scripts/runfvp
build/tmp/deploy/images/corstone1000-fvp/corstone1000-flash-firmware-image-corstone1000-fvp.fvpconf
```

Then run:

```sh
ctest --test-dir cmake-build-corstone-1000-aarch64 --output-on-failure
```

If the FVP root/workspace or Python runner dependencies such as `telnetlib3`
are missing, the FVP tests are marked skipped with a prerequisite message
instead of failing through an unrelated application-output regex.

## Running on Target

The MMC image is mounted under:

```text
/run/media/mmcblk0
```

Generic inference runner:

```sh
cd /run/media/mmcblk0
LD_LIBRARY_PATH=./lib ./bin/direct-drive-inference-runner \
  --model ./models/mobilenet_v2_1.0_224_INT8_vela_Z2048.tflite \
  --input-bin ./inputs/zero_224x224x3_u8.bin \
  --output ./outputs/output.bin \
  --lib ./lib/libethosu_op_delegate.so \
  --profiling \
  --cycles
```

Image classification runner:

```sh
cd /run/media/mmcblk0
LD_LIBRARY_PATH=./lib ./bin/direct-drive-image-classification \
  --model ./models/mobilenet_v2_1.0_224_INT8_vela_Z2048.tflite \
  --input-image ./inputs/tiger.bmp \
  --labels ./labels/labels_mobilenet_v2_1.0_224.txt \
  --output ./outputs/classification.bin \
  --lib ./lib/libethosu_op_delegate.so \
  --profiling \
  --cycles
```

Object detection runner:

```sh
cd /run/media/mmcblk0
LD_LIBRARY_PATH=./lib ./bin/direct-drive-object-detection \
  --model ./models/yolo-fastest_192_face_v4_vela_Z2048.tflite \
  --input-image ./inputs/couple.bmp \
  --output ./outputs/detections.bin \
  --lib ./lib/libethosu_op_delegate.so \
  --profiling \
  --cycles
```

For CPU/reference-kernel runs, omit `--lib`. To force TensorFlow™ Lite reference
kernels where supported, pass `--ref`.

With delegate execution, `--cycles` requests the Arm® Ethos™-U NPU PMU cycle
counter. When the target supports it, the delegate logs a line similar to:

```text
INFO: EthosuOp: PMU cycle counter = 1809653
```

Additional PMU event counters can be requested with `--pmu <counter> <event-id>`.
The event ID is the U85 driver lookup-table index. For example, `--pmu 0 4`
requests `PMU_EVENT_NPU_ACTIVE` on counter 0.

## Outputs

Write outputs under:

```text
/run/media/mmcblk0/outputs
```

The output directory is created by the packaging helper. Files written there are
stored on the MMC image and remain available after the FVP exits.

## Troubleshooting

If the target cannot find the delegate, check `LD_LIBRARY_PATH=./lib` and verify
that `lib/libethosu_op_delegate.so` exists in the MMC image.

If the application reports that no input was provided, use either `--input-bin`
or `--input-image`. These modes are mutually exclusive. The image input path
supports P6 PPM and uncompressed 24-bit BMP files. The older `--input-ppm`
option is kept as a compatibility alias.

If image classification reports that no labels were provided, pass a text file
with one label per output class using `--labels`. The default packaging target
places labels under `/labels`.

If inference fails inside the delegate, confirm that the model is a COP2 Vela
model compatible with the FVP image.

## Trademarks

* Arm, Corstone, Cortex, and Ethos are registered trademarks or trademarks of
  Arm Limited (or its subsidiaries) in the US and/or elsewhere.
* TensorFlow, the TensorFlow logo, and any related marks are trademarks of
  Google Inc.
* Yocto Project is a trademark of The Linux Foundation.
* All other trademarks are the property of their respective owners.
