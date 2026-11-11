# RHIZO Engine

RHIZO is the native Android ARM64 3D runtime for Flux Studio. Flux owns editor UI, timeline and 2D composition; RHIZO owns the 3D runtime.

## Implemented foundation
- Android arm64-v8a CMake shared-library target.
- Versioned C ABI with opaque context ownership and result codes.
- Vulkan instance creation and physical-device enumeration.
- Graphics queue selection, logical-device creation and resettable command pool.
- Exposes physical-device vendor/device IDs, API/driver versions and device name.

## Build
Configure with the Android NDK CMake toolchain:
    cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-android --parallel

The Linux Codespace is a cross-compilation host. Vulkan runtime validation must happen on the Android device.
Public C ABI: include/rhizo/rhizo.h. No C++ exceptions or STL objects cross the ABI boundary.
