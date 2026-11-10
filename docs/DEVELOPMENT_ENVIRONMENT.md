# RHIZO development environment

RHIZO is developed as an Android ARM64 native engine. The Codespace is a **build environment**, not a substitute for testing on a Mali device.

## Create the Codespace

1. Open the RHIZO repository on GitHub.
2. Select **Code → Codespaces → Create codespace on main**.
3. Wait for the container build and the Android SDK/NDK post-create setup to finish.
4. The post-start command prints the detected toolchain versions.

## Installed toolchain

- Ubuntu 24.04 development container
- CMake and Ninja
- Clang/LLVM, LLD, clang-format and clang-tidy
- OpenJDK 17
- Android SDK platform 35 and Build Tools 35.0.0
- Android NDK 28.2.13676358 (ARM64 target)
- Android CMake 3.31.6
- Vulkan headers/tools for host-side development
- glslang, glslc and SPIR-V tools
- Assimp, GLM and stb development packages

## Verify manually

```bash
bash .devcontainer/verify-toolchain.sh
```

Useful environment variables:

- `ANDROID_HOME=/opt/android-sdk`
- `ANDROID_SDK_ROOT=/opt/android-sdk`
- `ANDROID_NDK_HOME=/opt/android-sdk/ndk/28.2.13676358`

## Important GPU limitation

GitHub Codespaces does not expose the phone's Mali GPU. It can compile Android ARM64 native code and compile/validate SPIR-V modules, but it cannot prove that Vulkan initialization, extensions, shader execution, synchronization or performance work on the Realme device. Those checks must be run on the target phone and captured in RHIZO runtime logs.

The engine target remains Android `arm64-v8a`; do not add a 32-bit Android or desktop renderer as a fallback target.
