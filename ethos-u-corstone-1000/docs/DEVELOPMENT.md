# Development

This page collects local checks and development conventions for the
Arm® Corstone™-1000 examples.

## Python Development Environment

The repository root has a local virtual environment used for development tools:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r ethos-u-corstone-1000/requirements-dev.txt
```

The current dev dependency set includes a pinned clang-tidy package.

## clang-tidy

The project configuration lives at:

```text
ethos-u-corstone-1000/.clang-tidy
```

Run clang-tidy using the native compile database:

```sh
.venv/bin/clang-tidy \
  -p ethos-u-corstone-1000/cmake-build-native \
  ethos-u-corstone-1000/app/src/fwk/tflite/TfliteModel.cpp
```

Run it over the project translation units listed in the compile database:

```sh
rg -o '"file": "[^"]+/ethos-u-corstone-1000/app/src/[^"]+\.(cpp|cc|cxx|c)"' \
  ethos-u-corstone-1000/cmake-build-native/compile_commands.json
```

Then pass the unique file list to clang-tidy.

The config intentionally enables `misc-include-cleaner` and
`cppcoreguidelines-pro-type-reinterpret-cast` as warnings first. They are useful
for surfacing include hygiene and questionable casts, but they are currently too
noisy to enforce as errors across the whole project.

## Build Directories

Do not run checks over `cmake-build-*` source trees. Those directories contain
generated files and third-party sources fetched by CMake. The `.clang-tidy`
configuration restricts diagnostics to:

```text
ethos-u-corstone-1000/app/src
```

and excludes:

```text
ethos-u-corstone-1000/cmake-build-*
```

## Script Checks

Python scripts should compile, helper unit tests should pass, and shell scripts
should pass both syntax checking and ShellCheck:

```sh
cd ethos-u-corstone-1000
python3 -m py_compile scripts/py/prep_mmc.py scripts/py/launch_fvp.py
cd scripts/py
python3 -m pytest
cd ../..

bash -n setup_model_resources.sh
shellcheck setup_model_resources.sh
```

## C++ Style Notes

Prefer direct includes for symbols used by a `.cpp` or `.hpp` file. Avoid
depending on transitive includes from TensorFlow™ Lite or MLEK headers.

Use `static_cast` for conversions from `void*` to the original typed pointer.
Use `reinterpret_cast` only for intentional byte views or API-defined opaque
payload conversions, and expect clang-tidy to flag those cases for review.

## Trademarks

* Arm and Corstone are registered trademarks or trademarks of Arm Limited
  (or its subsidiaries) in the US and/or elsewhere.
* TensorFlow, the TensorFlow logo, and any related marks are trademarks of Google Inc.
* All other trademarks are the property of their respective owners.
