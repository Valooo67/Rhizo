# RHIZO Engine

RHIZO is the native Android ARM64 3D runtime for Flux Studio. Flux owns editor UI, timeline and 2D composition; RHIZO owns the 3D runtime.

## Implemented foundation

- Android `arm64-v8a` CMake shared-library target.
- Versioned C ABI with opaque context and buffer handles.
- Vulkan instance creation, physical-device selection and logical-device creation.
- Graphics queue and transient/resettable command pool.
- Physical-device identification (vendor/device IDs, API/driver versions and name).
- Host-visible and device-local GPU buffer allocation.
- Staging-buffer uploads into device-local resources.
- Serialized immediate command submission with fence completion and transfer-to-GPU-read visibility barrier.
- Explicit cleanup of buffers owned by a context.

## Build

Configure with the Android NDK CMake toolchain:

```sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android --parallel
```

The Linux runner is a cross-compilation host. Vulkan runtime validation must happen on an Android device. Public C ABI: `include/rhizo/rhizo.h`. No C++ exceptions or STL objects cross the ABI boundary.

## Vulkan buffers

Supported usage flags: vertex, index, uniform, storage and indirect.

Memory mode is selected with an optional flag in `RhizoBufferCreateInfo.usage`:

- `RHIZO_BUFFER_CPU_VISIBLE`: host-visible memory; writes map the allocation and flush non-coherent ranges when required.
- `RHIZO_BUFFER_GPU_ONLY`: device-local memory; `rhizo_buffer_write` uploads through a temporary host-visible staging buffer and a Vulkan transfer command.
- If neither memory flag is specified, RHIZO keeps the original host-visible behavior for compatibility.
- Setting both memory flags is invalid.

Minimal lifecycle:

1. Create a `RhizoContext` with `rhizo_create`.
2. Fill `RhizoBufferCreateInfo` with `struct_size`, a usage mask, and byte size.
3. Call `rhizo_buffer_create`, then upload bytes with `rhizo_buffer_write`.
4. Call `rhizo_buffer_destroy` for individual resources. `rhizo_destroy` also releases all remaining buffers owned by that context.

Buffer handles are opaque and are not Vulkan handles. Device-local uploads currently use a synchronous one-shot transfer path; a persistent staging ring, suballocation and asynchronous frame scheduler belong to later renderer stages.
