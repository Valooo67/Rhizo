#!/usr/bin/env bash
set -Eeuo pipefail

echo "===== RHIZO REMOTE BUILD + COMPLETE C ABI VALIDATION ====="
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
find include src -maxdepth 4 -type f -print | sort
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
echo "===== ABI / EXPORTED API CHECK ====="
readelf -h build-android/librhizo.so | grep -E 'Class:|Machine:'
for symbol in rhizo_get_abi_version rhizo_create rhizo_get_device_info rhizo_get_last_error rhizo_buffer_create rhizo_buffer_write rhizo_buffer_get_size rhizo_buffer_destroy rhizo_destroy rhizo_scene_create_node rhizo_scene_destroy_node rhizo_scene_set_parent rhizo_scene_set_transform rhizo_scene_get_transform rhizo_scene_get_node_count rhizo_scene_get_node rhizo_scene_set_keyframe rhizo_scene_evaluate_animation rhizo_scene_import_model rhizo_scene_get_mesh_count rhizo_scene_get_mesh_info rhizo_scene_get_material_count rhizo_scene_get_material_info rhizo_shadow_map_create rhizo_shadow_map_destroy rhizo_editor_grid_vertex_count rhizo_editor_grid_vertices rhizo_editor_gizmo_vertex_count rhizo_editor_gizmo_vertices rhizo_scene_set_camera rhizo_scene_get_camera rhizo_scene_set_light rhizo_scene_get_light rhizo_scene_set_parameter_keyframe; do
  nm -D --defined-only build-android/librhizo.so | grep -q " ${symbol}$"
  echo "exported: ${symbol}"
done
echo
echo "===== BUILD OUTPUT ====="
ls -lh build-android/librhizo.so
file build-android/librhizo.so
sha256sum build-android/librhizo.so
echo "===== RHIZO SCENE + CAMERA/LIGHT + EDITOR GEOMETRY + SHADOW RESOURCE ABI: SUCCESS ====="
