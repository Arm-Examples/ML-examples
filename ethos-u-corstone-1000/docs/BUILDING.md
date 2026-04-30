# Building

This project uses CMake presets for the supported build configurations:

* `native`: host build without the delegate for Arm® Ethos™-U NPUs.

* `corstone-1000-aarch64`: AArch64 Linux® cross build for the Arm® Corstone™-1000
  reference package with an Arm® Cortex®-A320 processor FVP image, with
  Ethos-U NPU support enabled.

Run all commands from the project directory unless stated otherwise:

```sh
cd ethos-u-corstone-1000
```

## Prerequisites

Required for all builds:

* CMake 3.25 or newer.

* A C++17-capable compiler.

* Git and normal build tools for fetched dependencies.

The native preset can fetch MLEK and TensorFlow™ Lite automatically. To use
local checkouts while iterating, export these before configuring:

```sh
export MLEK_SOURCE_DIR=/path/to/ml-embedded-evaluation-kit
export TENSORFLOW_LITE_DIR=/path/to/tensorflow
```

The presets copy these environment variables into CMake cache variables at
configure time. If you configure without presets, pass the same values with
`-DMLEK_SOURCE_DIR=...` and `-DTENSORFLOW_LITE_DIR=...`.

CMake configure expects the project Python environment to already exist. Run
`./setup_model_resources.sh` first to create and populate `downloads/env`, or
pass `-DPYTHON_VENV=<path-to-existing-venv>`.

The `corstone-1000-aarch64` preset also needs:

* Bootlin AArch64 musl toolchain binaries on `PATH`.

* Optional local Ethos-U Linux driver stack checkout:

```sh
export ETHOS_U_NPU_LINUX_STACK_DIR=/path/to/ethos-u-linux-driver-stack
```

If `ETHOS_U_NPU_LINUX_STACK_DIR` is not set, CMake fetches the pinned driver
stack revision.

## Presets

List available presets:

```sh
cmake --list-presets
```

The preset build directories are:

* `cmake-build-native`

* `cmake-build-corstone-1000-aarch64`

Both presets export `compile_commands.json`, which is used by clang-tidy and
IDEs.

## CMake Configure Options

| Option | Default | Description |
| --- | --- | --- |
| `TARGET_PLATFORM` | `native` | Build target: `native` or `corstone-1000-aarch64`. |
| `MLEK_SOURCE_DIR` | empty | Existing MLEK source checkout; when empty, CMake fetches MLEK. |
| `TENSORFLOW_LITE_DIR` | empty | Existing TensorFlow source checkout; when empty, CMake fetches TensorFlow Lite. |
| `DOWNLOADS_DIR` | `downloads` | Model-resource directory produced by `setup_model_resources.py --downloads-dir`. |
| `PYTHON_VENV` | `<DOWNLOADS_DIR>/env` | Existing Python virtual environment used by project helper scripts. |
| `USE_CASE_BUILD` | `all` | Use cases to build, or `all`. |
| `CORSTONE1000_PACKAGE_MMC` | `OFF` for native, `ON` for Corstone | Package the Corstone-1000 MMC image. |
| `CORSTONE1000_FVP_TESTS_ENABLED` | `ON` when Corstone MMC packaging is enabled, otherwise `OFF` | Register MMC-backed FVP CTests. Requires `TARGET_PLATFORM=corstone-1000-aarch64` and `CORSTONE1000_PACKAGE_MMC=ON`. |
| `CORSTONE1000_MMC_LAYOUT_FILE` | `resources/mmc_layout.json` | JSON file defining the MMC image name and target-side directory layout. |
| `ETHOS_U_NPU_ENABLED` | `ON` for Corstone | Whether the target system includes an Ethos-U NPU. |
| `ETHOS_U_NPU_BUILD_DELEGATE` | `ON` for Corstone | Build `libethosu_op_delegate.so`. |
| `ETHOS_U_NPU_LINUX_STACK_DIR` | empty | Existing Ethos-U Linux driver stack checkout; when empty, CMake fetches it. |

## Native Build

Configure and build:

```sh
cmake --preset native
cmake --build --preset native --parallel
```

Configure, build, and run the native tests in one step:

```sh
CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)" CTEST_PARALLEL_LEVEL="$(nproc)" cmake --workflow --preset native
```

The native build creates:

* `cmake-build-native/bin/direct-drive-inference-runner`

* `cmake-build-native/bin/direct-drive-image-classification`

* `cmake-build-native/bin/direct-drive-object-detection`

The native binaries are useful for fast CLI and TensorFlow Lite development.
They do not build or use the Ethos-U delegate.

## Corstone-1000 AArch64 Build

Download a prebuilt AArch64 musl toolchain from:

* `https://toolchains.bootlin.com/releases_aarch64.html`

The validated archive so far is:

* `https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--musl--stable-2025.08-1.tar.xz`

Extract the archive and add its `bin/` directory to `PATH`:

```sh
tar -xf aarch64--musl--stable-2025.08-1.tar.xz
export PATH="$PWD/aarch64--musl--stable-2025.08-1/bin:${PATH}"
```

Then configure and build:

```sh
cmake --preset corstone-1000-aarch64
cmake --build --preset corstone-1000-aarch64 --parallel
```

The Corstone-1000 AArch64 build creates:

* `cmake-build-corstone-1000-aarch64/bin/direct-drive-inference-runner`

* `cmake-build-corstone-1000-aarch64/bin/direct-drive-image-classification`

* `cmake-build-corstone-1000-aarch64/bin/direct-drive-object-detection`

* `cmake-build-corstone-1000-aarch64/lib/libethosu_op_delegate.so`

* `cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img`

TensorFlow Lite cross builds need a host `flatc`. By default, the CMake
integration builds this host tool from the TensorFlow sources before
configuring TensorFlow Lite. To use an existing host `flatc`, configure with:

```sh
cmake --preset corstone-1000-aarch64 \
  -DTFLITE_HOST_TOOLS_DIR=/path/to/directory-containing-flatc
```

To compile-check the AArch64 applications without building the Ethos-U delegate,
disable MMC packaging as well:

```sh
cmake --preset corstone-1000-aarch64 \
  -DETHOS_U_NPU_BUILD_DELEGATE=OFF \
  -DCORSTONE1000_PACKAGE_MMC=OFF
cmake --build --preset corstone-1000-aarch64 --parallel
```

## Toolchain Notes

The `corstone-1000-aarch64` preset uses:

```text
scripts/cmake/toolchains/aarch64-bootlin-linux-musl.cmake
```

The toolchain file expects `aarch64-linux-*` tools on `PATH`, rejects non-musl
toolchains, and keeps host-side tools on the build machine.

## Common Problems

If CMake cannot find the AArch64 compiler, check that the Bootlin toolchain
`bin` directory is on `PATH`.

If the Corstone-1000 AArch64 build fails while looking for `flatc`, either allow the project to
build host tools from TensorFlow sources or pass `TFLITE_HOST_TOOLS_DIR`.

If a fetched dependency changed locally, remove the relevant `cmake-build-*`
directory and reconfigure.

## Trademarks

* Arm, Corstone, Cortex, and Ethos are registered trademarks or trademarks of
  Arm Limited (or its subsidiaries) in the US and/or elsewhere.
* Linux® is the registered trademark of Linus Torvalds in the US and other countries.
* TensorFlow, the TensorFlow logo, and any related marks are trademarks of
  Google Inc.
* All other trademarks are the property of their respective owners.
