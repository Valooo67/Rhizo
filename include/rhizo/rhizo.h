#ifndef RHIZO_RHIZO_H
#define RHIZO_RHIZO_H
#include <stdint.h>
#if defined(_WIN32)
  #if defined(RHIZO_BUILDING_LIBRARY)
    #define RHIZO_API __declspec(dllexport)
  #else
    #define RHIZO_API __declspec(dllimport)
  #endif
#else
  #define RHIZO_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define RHIZO_ABI_VERSION 1u
typedef struct RhizoContext RhizoContext;
typedef struct RhizoBuffer RhizoBuffer;
typedef struct RhizoTexture RhizoTexture;
typedef enum RhizoResult {
  RHIZO_SUCCESS=0, RHIZO_ERROR_INVALID_ARGUMENT=1, RHIZO_ERROR_OUT_OF_MEMORY=2,
  RHIZO_ERROR_VULKAN=3, RHIZO_ERROR_UNSUPPORTED_DEVICE=4, RHIZO_ERROR_INTERNAL=5
} RhizoResult;
typedef struct RhizoCreateInfo {
  uint32_t struct_size,abi_version;
  const char* application_name;
  uint32_t requested_vulkan_api;
  uint8_t enable_validation,reserved[3];
} RhizoCreateInfo;
typedef enum RhizoBufferUsage {
  RHIZO_BUFFER_VERTEX=1u<<0, RHIZO_BUFFER_INDEX=1u<<1, RHIZO_BUFFER_UNIFORM=1u<<2,
  RHIZO_BUFFER_STORAGE=1u<<3, RHIZO_BUFFER_INDIRECT=1u<<4,
  RHIZO_BUFFER_CPU_VISIBLE=1u<<5, RHIZO_BUFFER_GPU_ONLY=1u<<6
} RhizoBufferUsage;
typedef struct RhizoBufferCreateInfo { uint32_t struct_size,usage; uint64_t size; } RhizoBufferCreateInfo;
typedef struct RhizoDeviceInfo {
  uint32_t struct_size,vendor_id,device_id,api_version,driver_version,device_type;
  char device_name[256],driver_name[256];
} RhizoDeviceInfo;
typedef struct RhizoTransform { float position[3],rotation_xyzw[4],scale[3]; } RhizoTransform;
typedef struct RhizoSceneNodeInfo {
  uint32_t struct_size,entity_id,parent_id,child_count,mesh_count;
  uint8_t visible;
  char name[128];
} RhizoSceneNodeInfo;
typedef struct RhizoImportReport {
  uint32_t struct_size,node_count,mesh_count,material_count;
  char resolved_model_path[512];
} RhizoImportReport;
typedef struct RhizoMeshInfo {
  uint32_t struct_size,vertex_count,index_count,material_index;
  char name[128];
} RhizoMeshInfo;
typedef enum RhizoInterpolation { RHIZO_INTERPOLATION_STEP=0, RHIZO_INTERPOLATION_LINEAR=1, RHIZO_INTERPOLATION_SMOOTH=2, RHIZO_INTERPOLATION_CUBIC=3 } RhizoInterpolation;
typedef enum RhizoPlaybackMode { RHIZO_PLAY_ONCE=0, RHIZO_PLAY_LOOP=1, RHIZO_PLAY_PING_PONG=2 } RhizoPlaybackMode;
typedef struct RhizoAnimationClipInfo { uint32_t struct_size,clip_id,key_count; double duration; char name[128]; } RhizoAnimationClipInfo;
typedef struct RhizoMeshSkinInfo { uint32_t struct_size,bone_count,morph_target_count; } RhizoMeshSkinInfo;
typedef struct RhizoBoneInfo { uint32_t struct_size,bone_index,node_id; char name[128]; } RhizoBoneInfo;
typedef struct RhizoMorphTargetInfo { uint32_t struct_size,target_index; char name[128]; } RhizoMorphTargetInfo;
typedef enum RhizoAnimatedProperty {
  RHIZO_ANIM_POSITION=0, RHIZO_ANIM_ROTATION=1, RHIZO_ANIM_SCALE=2,
  RHIZO_ANIM_CAMERA_VERTICAL_FOV=3, RHIZO_ANIM_CAMERA_NEAR=4,
  RHIZO_ANIM_CAMERA_FAR=5, RHIZO_ANIM_CAMERA_ORTHOGRAPHIC_SIZE=6,
  RHIZO_ANIM_LIGHT_INTENSITY=7, RHIZO_ANIM_LIGHT_RANGE=8,
  RHIZO_ANIM_LIGHT_INNER_CONE=9, RHIZO_ANIM_LIGHT_OUTER_CONE=10,
  RHIZO_ANIM_LIGHT_COLOR=11, RHIZO_ANIM_MATERIAL_BASE_COLOR=12, RHIZO_ANIM_MATERIAL_METALLIC=13, RHIZO_ANIM_MATERIAL_ROUGHNESS=14, RHIZO_ANIM_MATERIAL_NORMAL_SCALE=15, RHIZO_ANIM_MATERIAL_OCCLUSION=16, RHIZO_ANIM_MORPH_WEIGHT_BASE=17
} RhizoAnimatedProperty;
typedef struct RhizoCameraInfo { uint32_t struct_size; float vertical_fov,near_plane,far_plane,orthographic_size; uint8_t orthographic,reserved[3]; } RhizoCameraInfo;
typedef struct RhizoLightInfo { uint32_t struct_size,type; float color[3],intensity,range,inner_cone,outer_cone; uint8_t casts_shadows,reserved[3]; } RhizoLightInfo;
typedef struct RhizoDecalInfo { uint32_t struct_size; float color[4],opacity,angle_fade; } RhizoDecalInfo;
typedef struct RhizoShadowMapInfo { uint32_t struct_size,width,height,format; } RhizoShadowMapInfo;
typedef struct RhizoEditorVertex { float position[3],color[4]; } RhizoEditorVertex;
typedef enum RhizoShadowMode { RHIZO_SHADOWS_DISABLED=0, RHIZO_SHADOWS_MAPS=1 } RhizoShadowMode;
typedef enum RhizoPostEffectFlags {
 RHIZO_POST_BLOOM=1u<<0,RHIZO_POST_SSAO=1u<<1,RHIZO_POST_SSR=1u<<2,RHIZO_POST_SSGI=1u<<3,
 RHIZO_POST_DOF=1u<<4,RHIZO_POST_TAA=1u<<5,RHIZO_POST_MOTION_BLUR=1u<<6,RHIZO_POST_DECALS=1u<<7,
 RHIZO_POST_EFFECT_ALL=(1u<<8)-1u
} RhizoPostEffectFlags;
typedef struct RhizoRenderSettings {
 uint32_t struct_size,shadow_mode,shadow_resolution;
 float shadow_bias,shadow_normal_bias,exposure,environment_intensity,render_scale;
 uint8_t enable_pbr,enable_textures,enable_vsync,enable_backface_culling; uint32_t post_effects;
} RhizoRenderSettings;
typedef enum RhizoGizmoMode { RHIZO_GIZMO_TRANSLATE=0, RHIZO_GIZMO_ROTATE=1, RHIZO_GIZMO_SCALE=2 } RhizoGizmoMode;
typedef struct RhizoOpenGLFrameInfo {
 uint32_t struct_size,width,height,stride_bytes;
 uint64_t frame_index,timestamp_ns;
 uint32_t pixel_format,reserved;
} RhizoOpenGLFrameInfo;
enum { RHIZO_FRAME_RGBA8=1u, RHIZO_FRAME_BGRA8=2u };
typedef struct RhizoAssetInfo { uint32_t struct_size,type; uint64_t id,size,modified; char path[512]; } RhizoAssetInfo;
typedef struct RhizoResourceStats { uint32_t struct_size,texture_cache_entries,model_cache_entries,asset_count; } RhizoResourceStats;
typedef struct RhizoMaterialInfo {
  uint32_t struct_size;
  float base_color[4],metallic,roughness;
  char base_color_texture[512],normal_texture[512],metallic_roughness_texture[512],emissive_texture[512];
  float normal_scale,occlusion,emissive_color[3];
} RhizoMaterialInfo;
RHIZO_API uint32_t rhizo_get_abi_version(void);
RHIZO_API RhizoResult rhizo_create(const RhizoCreateInfo*,RhizoContext**);
RHIZO_API RhizoResult rhizo_get_device_info(const RhizoContext*,RhizoDeviceInfo*);
/* native_window is an Android ANativeWindow* passed as an opaque pointer. */
RHIZO_API RhizoResult rhizo_viewport_attach(RhizoContext*,void* native_window);
RHIZO_API RhizoResult rhizo_viewport_render(RhizoContext*,float red,float green,float blue,float alpha);
RHIZO_API RhizoResult rhizo_viewport_render_scene(RhizoContext*,const float view_projection[16]);
RHIZO_API RhizoResult rhizo_viewport_render_scene_camera(RhizoContext*,uint32_t camera_entity);
RHIZO_API RhizoResult rhizo_render_settings_set(RhizoContext*,const RhizoRenderSettings*);
RHIZO_API RhizoResult rhizo_render_settings_get(const RhizoContext*,RhizoRenderSettings*);
RHIZO_API RhizoResult rhizo_opengl_submit_frame(RhizoContext*,const RhizoOpenGLFrameInfo*,const void*,uint64_t);
RHIZO_API RhizoResult rhizo_opengl_get_latest_texture(const RhizoContext*,uint64_t*,uint64_t*,uint32_t*,uint32_t*);
RHIZO_API RhizoResult rhizo_opengl_get_latest_frame(const RhizoContext*,RhizoOpenGLFrameInfo*,const void**,uint64_t*);
RHIZO_API void rhizo_viewport_detach(RhizoContext*);
RHIZO_API const char* rhizo_get_last_error(const RhizoContext*);
RHIZO_API RhizoResult rhizo_buffer_create(RhizoContext*,const RhizoBufferCreateInfo*,RhizoBuffer**);
RHIZO_API RhizoResult rhizo_buffer_write(RhizoBuffer*,uint64_t,const void*,uint64_t);
RHIZO_API uint64_t rhizo_buffer_get_size(const RhizoBuffer*);
RHIZO_API void rhizo_buffer_destroy(RhizoBuffer*);
RHIZO_API void rhizo_destroy(RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_create_node(RhizoContext*,uint32_t,const char*,uint32_t*);
RHIZO_API RhizoResult rhizo_scene_destroy_node(RhizoContext*,uint32_t);
RHIZO_API RhizoResult rhizo_scene_set_parent(RhizoContext*,uint32_t,uint32_t);
RHIZO_API RhizoResult rhizo_scene_set_transform(RhizoContext*,uint32_t,const RhizoTransform*);
RHIZO_API RhizoResult rhizo_editor_set_selection(RhizoContext*,uint32_t);
RHIZO_API uint32_t rhizo_editor_get_selection(const RhizoContext*);
RHIZO_API RhizoResult rhizo_editor_set_gizmo_mode(RhizoContext*,uint32_t);
RHIZO_API uint32_t rhizo_editor_get_gizmo_mode(const RhizoContext*);
RHIZO_API RhizoResult rhizo_editor_apply_gizmo_delta(RhizoContext*,uint32_t axis,float delta,uint8_t local_space);
RHIZO_API RhizoResult rhizo_editor_gizmo_pick_axis(const float view_projection[16],uint32_t viewport_width,uint32_t viewport_height,const float origin[3],float world_size,float cursor_x,float cursor_y,float hit_radius_px,uint32_t* axis_out);
RHIZO_API RhizoResult rhizo_editor_gizmo_drag(RhizoContext*,const float view_projection[16],uint32_t viewport_width,uint32_t viewport_height,uint32_t axis,float world_size,float cursor_x,float cursor_y,float delta_x,float delta_y,uint8_t local_space);
RHIZO_API RhizoResult rhizo_scene_get_transform(const RhizoContext*,uint32_t,RhizoTransform*);
RHIZO_API uint32_t rhizo_scene_get_node_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_get_node(const RhizoContext*,uint32_t,RhizoSceneNodeInfo*);
RHIZO_API RhizoResult rhizo_scene_set_keyframe(RhizoContext*,uint32_t,uint32_t,double,const RhizoTransform*);
RHIZO_API RhizoResult rhizo_scene_evaluate_animation(RhizoContext*,double);
RHIZO_API RhizoResult rhizo_scene_set_parameter_keyframe(RhizoContext*,uint32_t,uint32_t,double,const float[4]);
RHIZO_API RhizoResult rhizo_scene_remove_keyframe(RhizoContext*,uint32_t,uint32_t,double);
RHIZO_API RhizoResult rhizo_scene_set_keyframe_interpolation(RhizoContext*,uint32_t,uint32_t,uint32_t);
RHIZO_API RhizoResult rhizo_scene_set_parameter_interpolation(RhizoContext*,uint32_t,uint32_t,uint32_t);
RHIZO_API RhizoResult rhizo_animation_play(RhizoContext*,uint8_t);
RHIZO_API RhizoResult rhizo_animation_pause(RhizoContext*);
RHIZO_API RhizoResult rhizo_animation_stop(RhizoContext*);
RHIZO_API RhizoResult rhizo_animation_seek(RhizoContext*,double);
RHIZO_API RhizoResult rhizo_animation_set_speed(RhizoContext*,double);
RHIZO_API RhizoResult rhizo_animation_set_mode(RhizoContext*,uint32_t);
RHIZO_API RhizoResult rhizo_animation_tick(RhizoContext*,double);
RHIZO_API double rhizo_animation_get_time(const RhizoContext*);
RHIZO_API uint32_t rhizo_animation_get_key_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_look_at(RhizoContext*,uint32_t,uint32_t,float);
RHIZO_API RhizoResult rhizo_scene_solve_two_bone_ik(RhizoContext*,uint32_t,uint32_t,uint32_t,const float[3],const float[3],float);
RHIZO_API RhizoResult rhizo_camera_shake(RhizoContext*,uint32_t,double,float,float);
RHIZO_API RhizoResult rhizo_camera_shake_clear(RhizoContext*,uint32_t);
RHIZO_API RhizoResult rhizo_animation_clip_create(RhizoContext*,uint32_t);
RHIZO_API RhizoResult rhizo_animation_clip_remove(RhizoContext*,uint32_t);
RHIZO_API uint32_t rhizo_animation_clip_get_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_animation_clip_get_info(const RhizoContext*,uint32_t,RhizoAnimationClipInfo*);
RHIZO_API RhizoResult rhizo_animation_clip_set_interpolation(RhizoContext*,uint32_t,uint32_t,uint32_t,uint32_t);
RHIZO_API RhizoResult rhizo_animation_clip_remove_keyframe(RhizoContext*,uint32_t,uint32_t,uint32_t,double);
RHIZO_API RhizoResult rhizo_animation_play_clip(RhizoContext*,uint32_t,uint8_t);
RHIZO_API RhizoResult rhizo_animation_clip_retarget(RhizoContext*,uint32_t,uint32_t,const uint32_t*,const uint32_t*,uint32_t);
RHIZO_API RhizoResult rhizo_animation_clip_set_keyframe(RhizoContext*,uint32_t,uint32_t,uint32_t,double,const RhizoTransform*);
RHIZO_API RhizoResult rhizo_animation_clip_set_parameter_keyframe(RhizoContext*,uint32_t,uint32_t,uint32_t,double,const float[4]);
RHIZO_API RhizoResult rhizo_animation_state_add(RhizoContext*,uint32_t,uint32_t,double);
RHIZO_API RhizoResult rhizo_animation_state_transition(RhizoContext*,uint32_t,uint32_t,double);
RHIZO_API RhizoResult rhizo_animation_state_play(RhizoContext*,uint32_t,uint8_t);
RHIZO_API RhizoResult rhizo_animation_state_tick(RhizoContext*,double);
RHIZO_API RhizoResult rhizo_scene_import_model(RhizoContext*,const char*,const char*,RhizoImportReport*);
RHIZO_API uint32_t rhizo_scene_get_mesh_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_get_mesh_info(const RhizoContext*,uint32_t,RhizoMeshInfo*);
RHIZO_API uint32_t rhizo_scene_get_material_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_get_mesh_skin_info(const RhizoContext*,uint32_t,RhizoMeshSkinInfo*);
RHIZO_API RhizoResult rhizo_scene_get_mesh_bone_info(const RhizoContext*,uint32_t,uint32_t,RhizoBoneInfo*);
RHIZO_API RhizoResult rhizo_scene_get_mesh_morph_info(const RhizoContext*,uint32_t,uint32_t,RhizoMorphTargetInfo*);
RHIZO_API RhizoResult rhizo_scene_set_morph_weight(RhizoContext*,uint32_t,uint32_t,float);
RHIZO_API RhizoResult rhizo_scene_get_morph_weight(const RhizoContext*,uint32_t,uint32_t,float*);
RHIZO_API RhizoResult rhizo_scene_set_camera(RhizoContext*,uint32_t,const RhizoCameraInfo*);
RHIZO_API RhizoResult rhizo_scene_get_camera(const RhizoContext*,uint32_t,RhizoCameraInfo*);
RHIZO_API RhizoResult rhizo_scene_set_light(RhizoContext*,uint32_t,const RhizoLightInfo*);
RHIZO_API RhizoResult rhizo_scene_get_light(const RhizoContext*,uint32_t,RhizoLightInfo*);
RHIZO_API RhizoResult rhizo_scene_set_decal(RhizoContext*,uint32_t,const RhizoDecalInfo*);
RHIZO_API RhizoResult rhizo_scene_remove_decal(RhizoContext*,uint32_t);
RHIZO_API RhizoResult rhizo_shadow_map_create(RhizoContext*,uint32_t,uint32_t,RhizoShadowMapInfo*);
RHIZO_API void rhizo_shadow_map_destroy(RhizoContext*);
RHIZO_API uint32_t rhizo_editor_grid_vertex_count(float,uint32_t);
RHIZO_API RhizoResult rhizo_editor_grid_vertices(float,uint32_t,RhizoEditorVertex*,uint32_t);
RHIZO_API uint32_t rhizo_editor_gizmo_vertex_count(void);
RHIZO_API RhizoResult rhizo_editor_gizmo_vertices(const float[3],float,RhizoEditorVertex*,uint32_t);

RHIZO_API RhizoResult rhizo_scene_get_material_info(const RhizoContext*,uint32_t,RhizoMaterialInfo*);
RHIZO_API RhizoResult rhizo_scene_set_material_info(RhizoContext*,uint32_t,const RhizoMaterialInfo*);
RHIZO_API RhizoResult rhizo_scene_save(RhizoContext*,const char*);
RHIZO_API RhizoResult rhizo_scene_load(RhizoContext*,const char*);
RHIZO_API RhizoResult rhizo_prefab_save(RhizoContext*,uint32_t,const char*);
RHIZO_API RhizoResult rhizo_prefab_instantiate(RhizoContext*,const char*,uint32_t,uint32_t*);
RHIZO_API RhizoResult rhizo_assets_scan(RhizoContext*,const char*);
RHIZO_API uint32_t rhizo_assets_get_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_assets_get(const RhizoContext*,uint32_t,RhizoAssetInfo*);
RHIZO_API void rhizo_resources_collect(RhizoContext*);
RHIZO_API void rhizo_resources_clear(RhizoContext*);
RHIZO_API RhizoResult rhizo_resources_get_stats(const RhizoContext*,RhizoResourceStats*);
RHIZO_API RhizoResult rhizo_scene_component_set(RhizoContext*,uint32_t,uint32_t,const void*,uint32_t);
RHIZO_API RhizoResult rhizo_scene_component_get(const RhizoContext*,uint32_t,uint32_t,void*,uint32_t,uint32_t*);
RHIZO_API RhizoResult rhizo_scene_component_remove(RhizoContext*,uint32_t,uint32_t);
RHIZO_API uint8_t rhizo_scene_component_has(const RhizoContext*,uint32_t,uint32_t);
RHIZO_API RhizoResult rhizo_texture_load(RhizoContext*,const char*,RhizoTexture**);
RHIZO_API RhizoResult rhizo_texture_get_info(const RhizoTexture*,uint32_t*,uint32_t*,uint64_t*);
RHIZO_API RhizoResult rhizo_texture_copy_pixels(const RhizoTexture*,void*,uint64_t);
RHIZO_API void rhizo_texture_release(RhizoTexture*);
#ifdef __cplusplus
}
#endif
#endif
