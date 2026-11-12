#!/usr/bin/env bash
set -Eeuo pipefail

echo "===== RHIZO REMOTE TERMINAL ====="
echo "UTC: $(date -u +%FT%TZ)"
echo "Host: $(uname -a)"
echo
echo "===== TOOLCHAIN ====="
cmake --version
ninja --version
clang --version | head -n 1
java -version
glslc --version
spirv-val --version
echo "ANDROID_HOME=${ANDROID_HOME:-unset}"
test -d "${ANDROID_HOME}/ndk/28.2.13676358"
echo "NDK 28.2.13676358: present"
echo
echo "===== SOURCE TREE ====="
find include src -maxdepth 3 -type f -print | sort
echo
echo "===== CMAKE CONFIGURE (Android ARM64) ====="
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
echo
echo "===== BUILD librhizo.so ====="
cmake --build build-android --parallel 2
echo
echo "===== BUILD OUTPUT ====="
ls -lh build-android/librhizo.so
file build-android/librhizo.so
echo "===== RHIZO REMOTE BUILD: SUCCESS ====="
