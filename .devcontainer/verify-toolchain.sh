#!/usr/bin/env bash
set -euo pipefail

echo "=== RHIZO Codespace toolchain ==="
printf "CMake: "; cmake --version | sed -n '1p'
printf "Ninja: "; ninja --version
printf "Clang: "; clang --version | sed -n '1p'
printf "Java: "; java -version 2>&1 | sed -n '1p'
printf "GLSL compiler: "; glslc --version | sed -n '1p'
printf "SPIR-V validator: "; spirv-val --version | sed -n '1p'
printf "Vulkan tools: "; vulkaninfo --summary 2>&1 | sed -n '1p' || true

if [[ -x "${ANDROID_HOME:-/opt/android-sdk}/ndk/28.2.13676358/ndk-build" ]]; then
  echo "Android NDK: 28.2.13676358"
else
  echo "Android NDK: not installed yet"
fi
