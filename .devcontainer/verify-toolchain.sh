#!/usr/bin/env bash
set -u

echo "=== RHIZO Codespace toolchain ==="
failed=0

check_line() {
  local label="$1"
  shift
  printf "%s: " "$label"
  if command -v "$1" >/dev/null 2>&1; then
    "$@" 2>&1 | sed -n '1p' || {
      echo "FAILED"
      failed=1
    }
  else
    echo "NOT FOUND"
    failed=1
  fi
}

check_line "CMake" cmake --version
check_line "Ninja" ninja --version
check_line "Clang" clang --version
check_line "Java" java -version
check_line "GLSL compiler" glslc --version
check_line "SPIR-V validator" spirv-val --version

printf "Vulkan tools: "
if command -v vulkaninfo >/dev/null 2>&1; then
  vulkaninfo --summary 2>&1 | sed -n '1p' || echo "available, but no summary"
else
  echo "NOT FOUND"
  failed=1
fi

NDK_ROOT="${ANDROID_HOME:-/opt/android-sdk}/ndk/28.2.13676358"
if [[ -x "$NDK_ROOT/ndk-build" ]]; then
  echo "Android NDK: 28.2.13676358"
else
  echo "Android NDK: NOT FOUND at $NDK_ROOT"
  failed=1
fi

if [[ "$failed" -eq 0 ]]; then
  echo "=== RHIZO toolchain check: OK ==="
else
  echo "=== RHIZO toolchain check: INCOMPLETE (see NOT FOUND/FAILED above) ==="
fi

# Diagnostic only: missing optional tools must not block Codespace startup.
exit 0
