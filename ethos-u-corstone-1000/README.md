# Arm® Ethos™-U85 Direct Drive on Arm® Corstone™-1000 with Arm® Cortex®-A320

This directory is the home for ML examples targeting the Arm® Corstone™-1000
reference package with Arm® Cortex®-A320 and a direct connection to the
Arm® Ethos™-U85 NPU.

The intent is to provide a small set of practical direct-drive examples that run
on the Corstone-1000 with Cortex-A320 FVP. The initial focus is a shared
userspace inference path on top of TensorFlow™ Lite, the Ethos-U delegate, and
MLEK's `fwk/iface` abstraction.

## Prerequisites

Build prerequisites are listed in [docs/BUILDING.md](docs/BUILDING.md#prerequisites).
All builds require CMake 3.25 or newer.

Before running these examples on the FVP, build the Corstone-1000 with
Cortex-A320 FVP and full BSP by following the guidance at
[corstone1000.docs.arm.com/en/corstone1000-2025.12](https://corstone1000.docs.arm.com/en/corstone1000-2025.12/).
N.B. the `corstone1000-2025.12` tagged version of the project is currently required.

For Corstone-1000 AArch64 builds, put the Bootlin AArch64 musl toolchain on
`PATH`.

## Model resources

Model resources are prepared by a thin project wrapper around the
pip-installable MLEK Python tooling. The shell wrapper creates a local virtual
environment under `downloads/env/`, installs the packages listed in
`scripts/py/requirements.txt`, downloads the resources listed in
`resources/model_manifest.json`, and runs Vela for the default Corstone-1000
Ethos-U85 configuration.

```sh
./ethos-u-corstone-1000/setup_model_resources.sh --parallel 8
```

The first example manifest prepares MobileNetV2 image-classification resources
and YOLO object-detection resources. Cached and optimised files are written
under `ethos-u-corstone-1000/downloads/`. Sample BMP inputs and ImageNet labels
are downloaded from the pinned MLEK revision listed in the manifest.

The manifest records source URLs and SHA-256 checksums for downloaded
resources. It currently references Arm-hosted MobileNetV2 image-classification
resources, Arm-hosted sample inputs and labels, and the third-party
YOLO-Fastest face detection model from `emza-vs/ModelZoo`. Generated zero
tensors are created locally by the setup helper.

Useful options:

- `--skip-vela` downloads resources without model optimisation.
- `--additional-ethos-u-config-name <name>` adds another Vela target, for example
  `ethos-u85-512`.
- `--venv-dir <path>` chooses a virtual environment directory. If omitted, the
  wrapper uses `<downloads-dir>/env`.

If `mlek-tools` is already installed in the active Python environment,
`scripts/py/setup_model_resources.py` can also be run directly.
CMake configure reuses this environment for project helper scripts. Run the
resource setup wrapper first, or pass `-DPYTHON_VENV=<path-to-existing-venv>`
when configuring.

## Expected scope

Detailed workflow documentation is available under `docs/`:

- `docs/BUILDING.md`
- `docs/PACKAGING.md`
- `docs/DEPLOYING.md`
- `docs/DEVELOPMENT.md`

## Dependencies

Build prerequisites are listed in [docs/BUILDING.md](docs/BUILDING.md#prerequisites).
All builds require CMake 3.25 or newer.

The project provides CMake presets for the common native and Corstone-1000 AArch64 builds.
Local source checkouts are optional; when they are not supplied, CMake fetches
the pinned MLEK and TensorFlow Lite revisions.

List available presets:

```sh
cd ethos-u-corstone-1000
cmake --list-presets
```

Common CMake configure options are listed in
[docs/BUILDING.md](docs/BUILDING.md#cmake-configure-options).

To use local MLEK and TensorFlow Lite checkouts while iterating, export:

```sh
export MLEK_SOURCE_DIR=/path/to/ml-embedded-evaluation-kit
export TENSORFLOW_LITE_DIR=/path/to/tensorflow
```

For Corstone-1000 AArch64 builds, a local Ethos-U Linux® driver stack checkout
can also be provided:

```sh
export ETHOS_U_NPU_LINUX_STACK_DIR=/path/to/ethos-u-linux-driver-stack
```

Native host build:

```sh
cd ethos-u-corstone-1000
cmake --preset native
cmake --build --preset native --parallel
```

To configure, build, and run the native tests in one step, use the native
workflow preset. Parallelism can be controlled through the standard CMake and
CTest environment variables:

```sh
CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)" CTEST_PARALLEL_LEVEL="$(nproc)" cmake --workflow --preset native
```

When `MLEK_SOURCE_DIR` is not set, CMake fetches MLEK from GitLab at the pinned
`MLEK_GIT_TAG`. When `TENSORFLOW_LITE_DIR` is not set, CMake fetches
TensorFlow from GitHub at the pinned `TENSORFLOW_GIT_TAG`.

## Corstone-1000 AArch64 Build

Configure and build the Corstone-1000 AArch64 target with the checked-in CMake
preset:

```sh
cd ethos-u-corstone-1000
cmake --preset corstone-1000-aarch64
cmake --build --preset corstone-1000-aarch64 --parallel
```

TensorFlow Lite cross builds also need a host `flatc`. The CMake integration
builds this from the TensorFlow sources before configuring TensorFlow Lite. To
use an existing host `flatc`, pass `TFLITE_HOST_TOOLS_DIR` pointing at the
directory containing `flatc`.

This build creates `bin/direct-drive-inference-runner`,
`bin/direct-drive-image-classification`, `bin/direct-drive-object-detection`,
`lib/libethosu_op_delegate.so`, and the MMC image used by the FVP helper:

```text
cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img
```

The Ethos-U Linux driver stack is fetched from Arm GitLab at the pinned
`ETHOS_U_NPU_LINUX_STACK_GIT_TAG` unless `ETHOS_U_NPU_LINUX_STACK_DIR` points
to an existing checkout.

To use a local driver-stack checkout while iterating:

```sh
cd ethos-u-corstone-1000
cmake --preset corstone-1000-aarch64 \
  -DETHOS_U_NPU_LINUX_STACK_DIR=/path/to/ethos-u-linux-driver-stack
cmake --build --preset corstone-1000-aarch64 --parallel
```

To compile-check the AArch64 applications without building the delegate, disable
MMC packaging as well:

```sh
cmake --preset corstone-1000-aarch64 \
  -DETHOS_U_NPU_BUILD_DELEGATE=OFF \
  -DCORSTONE1000_PACKAGE_MMC=OFF
cmake --build --preset corstone-1000-aarch64 --parallel
```

The `corstone-1000-aarch64` preset uses the checked-in Bootlin toolchain file at
`scripts/cmake/toolchains/aarch64-bootlin-linux-musl.cmake`. The toolchain file
uses `aarch64-linux-*` tools from `PATH`, rejects non-musl toolchains, and fails
at configure time if the expected tools are not available.

## Running on FVP

The helper needs a Yocto Project™ workspace for Corstone-1000 with a built full
BSP and `kas==4.4`. It uses the workspace's Corstone-1000 with Cortex-A320 FVP
executable by default; pass `--fvp` to override the executable path. To keep all
FVP consoles visible in one tmux window, add `--terminal-layout tmux-panes`.

```sh
ethos-u-corstone-1000/scripts/py/launch_fvp.py \
  --work-dir /path/to/corstone-1000-workspace \
  --mmc-image ethos-u-corstone-1000/cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img \
  --terminal-layout tmux-panes
```

On target, the MMC image is mounted under `/run/media/mmcblk0`.

Generic inference runner with the packaged MobileNetV2 model and zero tensor
input:

```sh
cd /run/media/mmcblk0
LD_LIBRARY_PATH=./lib ./bin/direct-drive-inference-runner \
  --model ./models/mobilenet_v2_1.0_224_INT8_vela_Z2048.tflite \
  --input-bin ./inputs/zero_224x224x3_u8.bin \
  --output ./outputs/inference-mobilenet-zero.bin \
  --lib ./lib/libethosu_op_delegate.so \
  --profiling \
  --cycles
```

Image classification with the packaged tiger sample and labels:

```sh
cd /run/media/mmcblk0
LD_LIBRARY_PATH=./lib ./bin/direct-drive-image-classification \
  --model ./models/mobilenet_v2_1.0_224_INT8_vela_Z2048.tflite \
  --input-image ./inputs/tiger.bmp \
  --labels ./labels/labels_mobilenet_v2_1.0_224.txt \
  --output ./outputs/classification-tiger.bin \
  --lib ./lib/libethosu_op_delegate.so \
  --profiling \
  --cycles
```

When `--cycles` is used with the Ethos-U delegate, the delegate logs the NPU PMU
cycle counter for the inference.

## Documentation

Detailed workflow documentation is available under [docs/](docs/):

- [docs/BUILDING.md](docs/BUILDING.md)
- [docs/PACKAGING.md](docs/PACKAGING.md)
- [docs/DEPLOYING.md](docs/DEPLOYING.md)
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)

## Trademarks

* Arm, Corstone, Cortex, and Ethos are registered trademarks or trademarks of
  Arm Limited (or its subsidiaries) in the US and/or elsewhere.
* Linux® is the registered trademark of Linus Torvalds in the US and other countries.
* TensorFlow, the TensorFlow logo, and any related marks are trademarks of
  Google Inc.
* Yocto Project is a trademark of The Linux Foundation.
* All other trademarks are the property of their respective owners.
