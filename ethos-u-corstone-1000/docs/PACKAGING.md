# Packaging

The Fixed Virtual Platform (FVP) for Arm® Corstone™-1000 can consume a
FAT MMC image containing the example executables, models, runtime
input files, labels, optional shared libraries, and an output directory. For the
AArch64 preset, CMake packages this image by default as:

```text
cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img
```

## Prerequisites

Install tools that provide these commands:

* `mcopy`, `mmd`, `mdel`, and `mdir` from `mtools`.
* `mkfs.vfat` from `dosfstools`, needed when creating a new image.
* `truncate`, usually available from GNU coreutils.

Model resources must be prepared before packaging:

```sh
./setup_model_resources.sh --parallel 8
```

No Yocto Project™ workspace for Corstone-1000 is needed to set up resources,
build the examples, or package the MMC image. The Yocto Project workspace is
only needed when launching the FVP.

## Image Layout

The helper creates or updates a 64 MiB FAT image with:

```text
/bin
/lib
/models
/inputs
/labels
/outputs
```

The default CMake packaging target copies:

* built example executables to `/bin`;
* `libethosu_op_delegate.so` to `/lib`;
* Vela-compiled `.tflite` files to `/models`;
* BMP and binary input files to `/inputs`;
* labels to `/labels`.

## CMake Packaging

For the Corstone-1000 AArch64 build, packaging runs as part of the default build:

```sh
cmake --preset corstone-1000-aarch64
cmake --build --preset corstone-1000-aarch64 --parallel
```

The packaging target is named `package-mmc`. It depends on `stage-mmc`, which
refreshes the `mmc-staging/` runtime files without updating the image. To
inspect the staged binaries and shared libraries before packaging, run:

```sh
cmake --build --preset corstone-1000-aarch64 --target stage-mmc
```

To build only the image after a configured build directory exists, run:

```sh
cmake --build --preset corstone-1000-aarch64 --target package-mmc
```

To disable automatic image packaging, configure with:

```sh
cmake --preset corstone-1000-aarch64 -DCORSTONE1000_PACKAGE_MMC=OFF
```

The layout JSON used by CMake packaging is controlled by
`CORSTONE1000_MMC_LAYOUT_FILE` and defaults to `resources/mmc_layout.json`.
That file defines the generated image name and the target-side directories
used for binaries, libraries, models, inputs, labels, outputs, and tests.

## Manual Packaging

The Python helper can also be run directly:

```sh
scripts/py/prep_mmc.py \
  --image cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img \
  --image-size 64MiB \
  --binary cmake-build-corstone-1000-aarch64/bin/direct-drive-inference-runner \
  --binary cmake-build-corstone-1000-aarch64/bin/direct-drive-image-classification \
  --binary cmake-build-corstone-1000-aarch64/bin/direct-drive-object-detection \
  --library cmake-build-corstone-1000-aarch64/lib/libethosu_op_delegate.so \
  --model 'downloads/img_class/*_vela_Z2048.tflite' \
  --model 'downloads/object_detection/*_vela_Z2048.tflite' \
  --input 'downloads/img_class/samples/*.bmp' \
  --input 'downloads/img_class/test-inputs/*.bin' \
  --input 'downloads/object_detection/samples/*.bmp' \
  --label 'downloads/img_class/labels/*.txt'
```

`--binary` and `--library` accept explicit file paths. The other role options
accept files or quoted globs. Globs are expanded by the Python helper at
packaging time, so missing resources fail with a direct error message.

Use `--image-size` to override the default 64 MiB image size. Automatic MMC
packaging uses 64 MiB. CMake packaging stages runtime files under
`mmc-staging/`; Debug staging copies are stripped with `--strip-debug`, while
the original build outputs remain unmodified.

Repeat `--model`, `--input`, `--label`, and `--library` when a use
case needs more than one file or glob for a runtime resource class.

## Model Compatibility

For Arm® Ethos™-U NPU delegate execution on the current Corstone-1000 with
Cortex-A320 FVP image, use a COP2 Vela model. Legacy COP1 payloads are rejected
by the current driver stack.

If delegate setup succeeds but inference fails with an Ethos-U network creation
error, check that the model was compiled by a Vela version and configuration
compatible with the Yocto Project image running in the FVP.

## Re-running Packaging

The helper updates an existing image in place and overwrites files with the
same names. If stale files become confusing, remove the image and recreate it:

```sh
rm -f cmake-build-corstone-1000-aarch64/corstone-1000-mmc.img
cmake --build --preset corstone-1000-aarch64 --target package-mmc
```

## Trademarks

* Arm, Corstone, Cortex, and Ethos are registered trademarks or trademarks of
  Arm Limited (or its subsidiaries) in the US and/or elsewhere.
* Yocto Project is a trademark of The Linux Foundation.
* All other trademarks are the property of their respective owners.
