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
- Vulkan swapchain viewport with indexed mesh rendering, depth testing, grid and transform-gizmo line geometry.
- Runtime render controls for exposure, environment intensity, PBR on/off and backface-culling on/off.
- OpenGL frame upload to a Vulkan sampled image, with an opaque image-view/sampler handle API for host-side composition.
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

The renderer currently provides the Vulkan viewport, PBR shading, depth-tested scene geometry, grid and gizmo visualization. The current shadow-map object allocates a sampled depth image, but a depth-only shadow render pass and shader-side shadow sampling are not yet connected; hardware ray tracing remains deferred. Material texture paths are imported and reported, but texture decoding, per-material GPU descriptors and sampling are still pending. OpenGL frames are uploaded to Vulkan images and exposed as handles; automatic compositing into scene materials is not yet wired. Device validation and end-to-end Flux Studio JNI/UI integration remain necessary. The animation API exposes entity transforms and generic parameter tracks.


## Stage 5 — Physics

RHIZO integrates Bullet Physics (zlib license) as its CPU-side simulation backend. The runtime exposes a C ABI for static, dynamic and kinematic rigid bodies; box, sphere, capsule and mesh colliders; per-body friction, restitution, density and damping; collision filtering; central forces and impulses; fixed, hinge, spring and distance constraints (multiple constraints per body); ray queries; and physics/animation drive modes.

Physics component descriptors are stored in the scene's custom-component payloads, so scene and prefab serialization retain body, joint and animation-binding configuration. After scene loading, RHIZO rebuilds the physics world from those persisted descriptors. Removing a body also removes constraints that reference it, preventing stale serialized links. Call `rhizo_physics_step` once per frame with elapsed seconds; the solver uses a bounded fixed substep and writes dynamic-body transforms back to the scene. Kinematic bodies follow scene/animation transforms. Dynamic mesh colliders use convex hulls; static mesh colliders use triangle meshes.

The simulation runs on the CPU and is independent of Vulkan rendering. Physical-device timing and collision behavior still require runtime testing on target hardware.
