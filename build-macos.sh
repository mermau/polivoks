#!/usr/bin/env bash
set -euo pipefail
# Run on macOS with Xcode, CMake >= 3.22 and internet access.
cd "$(dirname "$0")"
cmake -S . -B build-macos -G Xcode \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DPOLIVOKS_BUILD_PLUGIN=ON \
  -DPOLIVOKS_BUILD_TESTS=ON
cmake --build build-macos --config Release --parallel 4
ctest --test-dir build-macos -C Release --output-on-failure
printf '\nCompiled plug-ins, if build succeeded:\n'
find build-macos -type d \( -name '*.vst3' -o -name '*.component' \) -print
