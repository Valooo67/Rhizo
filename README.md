# RHIZO Engine

RHIZO is the native Android ARM64 3D runtime for Flux Studio. The engine exposes a stable C ABI so the Android application can call it from JNI without crossing the boundary with C++ objects.

## Implemented runtime foundation

- Android `arm64-v8a` shared-library target and versioned C ABI.
- Vulkan instance/device selection, graphics queue and command pool.
- Physical-device information.
- Host-visible and device-local GPU buffers with staging uploads and transfer visibility barriers.
- Scene graph with entity IDs, parent/child hierarchy, local transforms and recursive world matrices.
- Transform animation tracks: position, quaternion rotation (slerp) and scale.
- Assimp-based FBX, glTF/GLB, OBJ and DAE import.
- PBR material metadata: base color, metallic, roughness, base-color, normal, metallic and emissive texture references.
- Embedded material texture extraction from imported assets.
- ZIP package extraction for ordinary ZIP archives, with path traversal protection and a 512 MiB per-entry extraction limit.

## Current C ABI surface

Public header: `include/rhizo/rhizo.h`.

- Runtime and GPU: `rhizo_create`, `rhizo_get_device_info`, `rhizo_buffer_create`, `rhizo_buffer_write`.
- Hierarchy: `rhizo_scene_create_node`, `rhizo_scene_destroy_node`, `rhizo_scene_set_parent`, `rhizo_scene_get_node`.
- Transforms: `rhizo_scene_set_transform`, `rhizo_scene_get_transform`.
- Keyframes: `rhizo_scene_set_keyframe`, `rhizo_scene_evaluate_animation`.
- Assets: `rhizo_scene_import_model`, `rhizo_scene_get_mesh_info`, `rhizo_scene_get_material_info`.

The API is intentionally C-compatible for JNI. The application can traverse the scene tree by enumerating nodes and reading each node's parent ID, child count, mesh count and name.

## Model formats

- FBX, GLB/glTF, OBJ and DAE are decoded by Assimp.
- ZIP packages are extracted to the caller-provided cache directory. RHIZO selects a supported model file in the archive and resolves adjacent texture files. Embedded textures are copied to the cache.
- ZIP64 and unsupported ZIP compression methods are not currently supported. The archive is not treated as an arbitrary filesystem image.

## Build

```sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android --parallel
```

Assimp is fetched at configure time and built for the Android target. The Linux CI runner cross-compiles the native library; that does not replace runtime testing on the target Android GPU.

## Not yet complete

This is not yet the full viewport renderer. Android surface/swapchain presentation, PBR draw pipeline, shadow-map and hardware ray-query shadow backends, editor grid/gizmo draw passes, camera/light components, and end-to-end Flux Studio JNI/UI integration still require implementation and device validation. The current animation API covers entity transforms; arbitrary material/light/camera property tracks are not yet exposed.
