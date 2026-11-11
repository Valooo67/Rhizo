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


## Vulkan GPU buffers

RHIZO exposes host-visible Vulkan buffers through its versioned C ABI. Supported usages are vertex, index, uniform, storage, and indirect. A buffer can be initialized or updated with `rhizo_buffer_write`; writes validate their byte range and flush non-coherent mapped memory before returning.

Minimal lifecycle:

1. Create a `RhizoContext` with `rhizo_create`.
2. Fill `RhizoBufferCreateInfo` with `struct_size`, a non-zero usage mask, and byte size.
3. Call `rhizo_buffer_create`, then upload bytes with `rhizo_buffer_write`.
4. Destroy buffers with `rhizo_buffer_destroy` before destroying the context. Destroying the context also invalidates and releases any buffers still registered to it.

The current implementation allocates host-visible memory directly. This is a functional upload path for initial GPU integration; a device-local allocator and staging-transfer path are the next performance layer. Buffer handles are opaque and are not Vulkan handles.
