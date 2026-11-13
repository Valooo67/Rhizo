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
typedef enum RhizoAnimatedProperty {
  RHIZO_ANIM_POSITION=0, RHIZO_ANIM_ROTATION=1, RHIZO_ANIM_SCALE=2,
  RHIZO_ANIM_CAMERA_VERTICAL_FOV=3, RHIZO_ANIM_CAMERA_NEAR=4,
  RHIZO_ANIM_CAMERA_FAR=5, RHIZO_ANIM_CAMERA_ORTHOGRAPHIC_SIZE=6,
  RHIZO_ANIM_LIGHT_INTENSITY=7, RHIZO_ANIM_LIGHT_RANGE=8,
  RHIZO_ANIM_LIGHT_INNER_CONE=9, RHIZO_ANIM_LIGHT_OUTER_CONE=10,
  RHIZO_ANIM_LIGHT_COLOR=11, RHIZO_ANIM_MATERIAL_BASE_COLOR=12, RHIZO_ANIM_MATERIAL_METALLIC=13, RHIZO_ANIM_MATERIAL_ROUGHNESS=14, RHIZO_ANIM_MATERIAL_NORMAL_SCALE=15, RHIZO_ANIM_MATERIAL_OCCLUSION=16
} RhizoAnimatedProperty;
typedef struct RhizoCameraInfo { uint32_t struct_size; float vertical_fov,near_plane,far_plane,orthographic_size; uint8_t orthographic,reserved[3]; } RhizoCameraInfo;
typedef struct RhizoLightInfo { uint32_t struct_size,type; float color[3],intensity,range,inner_cone,outer_cone; uint8_t casts_shadows,reserved[3]; } RhizoLightInfo;
typedef struct RhizoShadowMapInfo { uint32_t struct_size,width,height,format; } RhizoShadowMapInfo;
typedef struct RhizoEditorVertex { float position[3],color[4]; } RhizoEditorVertex;
typedef enum RhizoShadowMode { RHIZO_SHADOWS_DISABLED=0, RHIZO_SHADOWS_MAPS=1 } RhizoShadowMode;
typedef struct RhizoRenderSettings {
 uint32_t struct_size,shadow_mode,shadow_resolution;
 float shadow_bias,shadow_normal_bias,exposure,environment_intensity,render_scale;
 uint8_t enable_pbr,enable_textures,enable_vsync,enable_backface_culling,reserved[4];
} RhizoRenderSettings;
typedef struct RhizoOpenGLFrameInfo {
 uint32_t struct_size,width,height,stride_bytes;
 uint64_t frame_index,timestamp_ns;
 uint32_t pixel_format,reserved;
} RhizoOpenGLFrameInfo;
enum { RHIZO_FRAME_RGBA8=1u, RHIZO_FRAME_BGRA8=2u };
typedef struct RhizoMaterialInfo {
  uint32_t struct_size;
  float base_color[4],metallic,roughness;
  char base_color_texture[512],normal_texture[512],metallic_roughness_texture[512],emissive_texture[512];
} RhizoMaterialInfo;
RHIZO_API uint32_t rhizo_get_abi_version(void);
RHIZO_API RhizoResult rhizo_create(const RhizoCreateInfo*,RhizoContext**);
RHIZO_API RhizoResult rhizo_get_device_info(const RhizoContext*,RhizoDeviceInfo*);
/* native_window is an Android ANativeWindow* passed as an opaque pointer. */
RHIZO_API RhizoResult rhizo_viewport_attach(RhizoContext*,void* native_window);
RHIZO_API RhizoResult rhizo_viewport_render(RhizoContext*,float red,float green,float blue,float alpha);
RHIZO_API RhizoResult rhizo_viewport_render_scene(RhizoContext*,const float view_projection[16]);
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
RHIZO_API RhizoResult rhizo_scene_get_transform(const RhizoContext*,uint32_t,RhizoTransform*);
RHIZO_API uint32_t rhizo_scene_get_node_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_get_node(const RhizoContext*,uint32_t,RhizoSceneNodeInfo*);
RHIZO_API RhizoResult rhizo_scene_set_keyframe(RhizoContext*,uint32_t,uint32_t,double,const RhizoTransform*);
RHIZO_API RhizoResult rhizo_scene_evaluate_animation(RhizoContext*,double);
RHIZO_API RhizoResult rhizo_scene_set_parameter_keyframe(RhizoContext*,uint32_t,uint32_t,double,const float[4]);
RHIZO_API RhizoResult rhizo_scene_import_model(RhizoContext*,const char*,const char*,RhizoImportReport*);
RHIZO_API uint32_t rhizo_scene_get_mesh_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_get_mesh_info(const RhizoContext*,uint32_t,RhizoMeshInfo*);
RHIZO_API uint32_t rhizo_scene_get_material_count(const RhizoContext*);
RHIZO_API RhizoResult rhizo_scene_set_camera(RhizoContext*,uint32_t,const RhizoCameraInfo*);
RHIZO_API RhizoResult rhizo_scene_get_camera(const RhizoContext*,uint32_t,RhizoCameraInfo*);
RHIZO_API RhizoResult rhizo_scene_set_light(RhizoContext*,uint32_t,const RhizoLightInfo*);
RHIZO_API RhizoResult rhizo_scene_get_light(const RhizoContext*,uint32_t,RhizoLightInfo*);
RHIZO_API RhizoResult rhizo_shadow_map_create(RhizoContext*,uint32_t,uint32_t,RhizoShadowMapInfo*);
RHIZO_API void rhizo_shadow_map_destroy(RhizoContext*);
RHIZO_API uint32_t rhizo_editor_grid_vertex_count(float,uint32_t);
RHIZO_API RhizoResult rhizo_editor_grid_vertices(float,uint32_t,RhizoEditorVertex*,uint32_t);
RHIZO_API uint32_t rhizo_editor_gizmo_vertex_count(void);
RHIZO_API RhizoResult rhizo_editor_gizmo_vertices(const float[3],float,RhizoEditorVertex*,uint32_t);

RHIZO_API RhizoResult rhizo_scene_get_material_info(const RhizoContext*,uint32_t,RhizoMaterialInfo*);
#ifdef __cplusplus
}
#endif
#endif
