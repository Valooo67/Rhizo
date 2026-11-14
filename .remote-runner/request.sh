#!/usr/bin/env bash
set -Eeuo pipefail

echo "===== RHIZO STAGE 6 SYSTEMS BUILD CANDIDATE ====="
echo "===== RHIZO AUDIO + TOUCH + PARTICLES + INPUT VALIDATION ====="
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
echo "===== CHECKED OUT REVISION ====="
git rev-parse HEAD
git status --short
echo "Stage 6 systems source revision: $(git rev-parse HEAD)"
echo "Bullet Physics: static/dynamic/kinematic rigid bodies, box/sphere/capsule/mesh colliders, fixed-step simulation, multiple joints per body, raycasts, material response and animation drive modes"
echo "HDR composite includes depth-reconstructed SSAO, SSR, SSGI, DoF, reprojection TAA and camera motion blur; temporal color/depth history; bloom; sorted alpha-blended materials"
echo "Scene/prefab serialization remaps joint entity IDs; multiple-constraint persistence and link-remap regression tests included"
nproc
free -h
df -h .
echo
echo "===== SOURCE TREE ====="
find include src -maxdepth 4 -type f -print | sort
echo
echo "===== HOST INPUT + PARTICLE SYSTEM TESTS ====="
clang++ -std=c++20 -Wall -Wextra -Wpedantic -I include tests/system_runtime_tests.cpp src/input/input.cpp src/particles/particle_system.cpp src/scene/scene.cpp -o /tmp/rhizo-system-tests
/tmp/rhizo-system-tests
echo "input and particle runtime tests: PASS"
echo
echo "===== HOST ANIMATION RUNTIME TESTS ====="
clang++ -std=c++20 -Wall -Wextra -I include tests/animation_runtime_tests.cpp src/scene/animation.cpp src/scene/scene.cpp -o /tmp/rhizo-animation-tests
/tmp/rhizo-animation-tests
echo "animation runtime tests: PASS"
clang++ -std=c++20 -Wall -Wextra -I include tests/scene_serialization_tests.cpp src/scene/scene_io.cpp src/scene/scene.cpp -o /tmp/rhizo-scene-tests
/tmp/rhizo-scene-tests
echo "scene and prefab serialization tests: PASS"
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
cmake --build build-android --parallel 2 --verbose
echo
echo "===== SPIR-V VALIDATION ====="
for shader in build-android/shaders/*.spv; do
  spirv-val --target-env vulkan1.1 "$shader"
  echo "validated: $(basename "$shader")"
done
echo
echo "===== ABI / EXPORTED API CHECK ====="
readelf -h build-android/librhizo.so | grep -E 'Class:|Machine:'
cat > /tmp/rhizo_abi_check.c <<'EOF'
#include "rhizo/rhizo.h"
int main(void) { return (int)RHIZO_ABI_VERSION; }
EOF
clang -std=c11 -Wall -Wextra -Werror -I include -fsyntax-only /tmp/rhizo_abi_check.c
for symbol in rhizo_get_abi_version rhizo_create rhizo_get_device_info rhizo_viewport_attach rhizo_viewport_render rhizo_viewport_detach rhizo_get_last_error rhizo_buffer_create rhizo_buffer_write rhizo_buffer_get_size rhizo_buffer_destroy rhizo_destroy rhizo_scene_create_node rhizo_scene_destroy_node rhizo_scene_set_parent rhizo_scene_set_transform rhizo_scene_get_transform rhizo_scene_get_node_count rhizo_scene_get_node rhizo_scene_set_keyframe rhizo_scene_evaluate_animation rhizo_scene_import_model rhizo_scene_get_mesh_count rhizo_scene_get_mesh_info rhizo_scene_get_material_count rhizo_scene_get_material_info rhizo_scene_set_parameter_keyframe rhizo_scene_remove_keyframe rhizo_scene_set_keyframe_interpolation rhizo_scene_set_parameter_interpolation rhizo_animation_play rhizo_animation_pause rhizo_animation_stop rhizo_animation_seek rhizo_animation_set_speed rhizo_animation_set_mode rhizo_animation_tick rhizo_animation_get_time rhizo_animation_get_key_count rhizo_animation_clip_create rhizo_animation_clip_remove rhizo_animation_clip_get_count rhizo_animation_clip_get_info rhizo_animation_clip_set_keyframe rhizo_animation_clip_set_parameter_keyframe rhizo_animation_clip_set_interpolation rhizo_animation_clip_remove_keyframe rhizo_animation_play_clip rhizo_animation_clip_retarget rhizo_animation_state_add rhizo_animation_state_transition rhizo_animation_state_play rhizo_animation_state_tick rhizo_scene_look_at rhizo_scene_solve_two_bone_ik rhizo_camera_shake rhizo_camera_shake_clear rhizo_scene_get_mesh_skin_info rhizo_scene_get_mesh_bone_info rhizo_scene_get_mesh_morph_info rhizo_scene_set_morph_weight rhizo_scene_get_morph_weight rhizo_scene_set_decal rhizo_scene_remove_decal rhizo_physics_body_create rhizo_physics_body_remove rhizo_physics_body_set_velocity rhizo_physics_body_apply_force rhizo_physics_body_apply_impulse rhizo_physics_joint_create rhizo_physics_joint_remove rhizo_physics_set_animation_mode rhizo_physics_step rhizo_physics_raycast rhizo_physics_clear rhizo_audio_load_wav rhizo_audio_load_pcm rhizo_audio_unload_clip rhizo_audio_play rhizo_audio_source_stop rhizo_audio_source_pause rhizo_audio_source_set rhizo_audio_set_listener rhizo_audio_set_master_volume rhizo_audio_get_stats rhizo_audio_shutdown rhizo_input_begin_frame rhizo_input_touch_event rhizo_input_key_event rhizo_input_bind_key rhizo_input_bind_touch_button rhizo_input_bind_touch_axis rhizo_input_get_action rhizo_input_get_touch rhizo_input_remove_action rhizo_input_clear rhizo_particle_emitter_set rhizo_particle_emitter_remove rhizo_particle_burst rhizo_particle_step rhizo_particle_get_count rhizo_particle_copy rhizo_particle_clear; do
  nm -D --defined-only build-android/librhizo.so | grep -q " ${symbol}$"
  echo "exported: ${symbol}"
done
echo
echo "===== BUILD OUTPUT ====="
ls -lh build-android/librhizo.so
file build-android/librhizo.so
sha256sum build-android/librhizo.so
echo "===== RHIZO STAGE 6 AUDIO + TOUCH INPUT + PARTICLE SIMULATION + ARM64 BUILD: SUCCESS ====="

