#include "rhizo/rhizo.h"

#include "vulkan/gpu_buffer.hpp"
#include "vulkan/vulkan_context.hpp"
#include "rhizo/scene.hpp"
#include "rhizo/animation.hpp"
#include "rhizo/model_importer.hpp"
#include "rhizo/shadow_map.hpp"
#include "rhizo/editor_geometry.hpp"
#include "rhizo/shadow_map.hpp"
#include "rhizo/editor_geometry.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

struct RhizoBuffer;

struct RhizoContext {
  rhizo::VulkanContext vulkan;
  std::string last_error;
  std::vector<RhizoBuffer*> buffers;
  rhizo::Scene scene;
  rhizo::Animation animation;
  std::unique_ptr<rhizo::ShadowMap> shadow_map;
  std::unique_ptr<rhizo::ShadowMap> shadow_map;
};

struct RhizoBuffer {
  RhizoContext* owner = nullptr;
  rhizo::GpuBuffer gpu;
};

namespace {

constexpr uint32_t kUsageMask =
    RHIZO_BUFFER_VERTEX | RHIZO_BUFFER_INDEX | RHIZO_BUFFER_UNIFORM |
    RHIZO_BUFFER_STORAGE | RHIZO_BUFFER_INDIRECT;

constexpr uint32_t kMemoryMask =
    RHIZO_BUFFER_CPU_VISIBLE | RHIZO_BUFFER_GPU_ONLY;

VkBufferUsageFlags to_vk_usage(uint32_t usage) {
  VkBufferUsageFlags flags = 0;
  if ((usage & RHIZO_BUFFER_VERTEX) != 0) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_INDEX) != 0) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_UNIFORM) != 0) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_STORAGE) != 0) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_INDIRECT) != 0) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  return flags;
}

}  // namespace

extern "C" {

uint32_t rhizo_get_abi_version(void) {
  return RHIZO_ABI_VERSION;
}

RhizoResult rhizo_create(const RhizoCreateInfo* info, RhizoContext** out_context) {
  if (out_context == nullptr) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }
  *out_context = nullptr;

  if (info == nullptr || info->struct_size < sizeof(RhizoCreateInfo) ||
      info->abi_version != RHIZO_ABI_VERSION) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  auto context = std::unique_ptr<RhizoContext>(new (std::nothrow) RhizoContext());
  if (!context) {
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  const char* application =
      info->application_name != nullptr && info->application_name[0] != '\0'
          ? info->application_name
          : "RHIZO";
  const uint32_t api = info->requested_vulkan_api != 0
      ? info->requested_vulkan_api
      : VK_API_VERSION_1_1;

  const rhizo::VulkanStatus status =
      context->vulkan.initialize(application, api);
  if (status != rhizo::VulkanStatus::Success) {
    context->last_error = context->vulkan.last_error();
    return status == rhizo::VulkanStatus::UnsupportedDevice
        ? RHIZO_ERROR_UNSUPPORTED_DEVICE
        : RHIZO_ERROR_VULKAN;
  }

  *out_context = context.release();
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_get_device_info(const RhizoContext* context,
                                  RhizoDeviceInfo* out_info) {
  if (context == nullptr || out_info == nullptr ||
      out_info->struct_size < sizeof(RhizoDeviceInfo)) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  const rhizo::VulkanDeviceInfo& source = context->vulkan.device_info();
  out_info->vendor_id = source.vendor_id;
  out_info->device_id = source.device_id;
  out_info->api_version = source.api_version;
  out_info->driver_version = source.driver_version;
  out_info->device_type = source.device_type;

  std::memset(out_info->device_name, 0, sizeof(out_info->device_name));
  std::memset(out_info->driver_name, 0, sizeof(out_info->driver_name));
  std::strncpy(out_info->device_name, source.device_name.c_str(),
               sizeof(out_info->device_name) - 1);
  std::strncpy(out_info->driver_name, source.driver_name.c_str(),
               sizeof(out_info->driver_name) - 1);
  return RHIZO_SUCCESS;
}

const char* rhizo_get_last_error(const RhizoContext* context) {
  return context == nullptr ? "Invalid RHIZO context" : context->last_error.c_str();
}

RhizoResult rhizo_buffer_create(RhizoContext* context,
                                const RhizoBufferCreateInfo* info,
                                RhizoBuffer** out_buffer) {
  if (out_buffer == nullptr) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }
  *out_buffer = nullptr;

  if (context == nullptr || info == nullptr ||
      info->struct_size < sizeof(RhizoBufferCreateInfo) ||
      info->size == 0 || info->size > static_cast<uint64_t>(SIZE_MAX) ||
      (info->usage & kUsageMask) == 0 ||
      (info->usage & ~(kUsageMask | kMemoryMask)) != 0 ||
      (info->usage & kMemoryMask) == kMemoryMask) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  const bool host_visible =
      (info->usage & RHIZO_BUFFER_GPU_ONLY) == 0;

  auto buffer = std::unique_ptr<RhizoBuffer>(new (std::nothrow) RhizoBuffer());
  if (!buffer) {
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  std::string error;
  if (!buffer->gpu.create(context->vulkan, static_cast<VkDeviceSize>(info->size),
                          to_vk_usage(info->usage), host_visible, error)) {
    context->last_error = std::move(error);
    return RHIZO_ERROR_VULKAN;
  }

  buffer->owner = context;
  try {
    context->buffers.push_back(buffer.get());
  } catch (...) {
    context->last_error = "Unable to register RHIZO buffer";
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  *out_buffer = buffer.release();
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_buffer_write(RhizoBuffer* buffer, uint64_t offset,
                               const void* data, uint64_t size) {
  if (buffer == nullptr || buffer->owner == nullptr ||
      offset > static_cast<uint64_t>(SIZE_MAX) ||
      size > static_cast<uint64_t>(SIZE_MAX)) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  std::string error;
  if (!buffer->gpu.write(static_cast<VkDeviceSize>(offset), data,
                         static_cast<VkDeviceSize>(size), error)) {
    buffer->owner->last_error = std::move(error);
    return RHIZO_ERROR_VULKAN;
  }
  return RHIZO_SUCCESS;
}

uint64_t rhizo_buffer_get_size(const RhizoBuffer* buffer) {
  return buffer == nullptr ? 0 : static_cast<uint64_t>(buffer->gpu.size());
}

void rhizo_buffer_destroy(RhizoBuffer* buffer) {
  if (buffer == nullptr) {
    return;
  }

  if (buffer->owner != nullptr) {
    auto& buffers = buffer->owner->buffers;
    buffers.erase(std::remove(buffers.begin(), buffers.end(), buffer), buffers.end());
    buffer->owner = nullptr;
  }
  delete buffer;
}

void rhizo_destroy(RhizoContext* context) {
  if (context == nullptr) {
    return;
  }

  for (RhizoBuffer* buffer : context->buffers) {
    delete buffer;
  }
  context->buffers.clear();
  delete context;
}


RhizoResult rhizo_scene_create_node(RhizoContext* c,uint32_t parent,const char* name,uint32_t* out){
  if(!c||!out||(parent!=0&&!c->scene.get(parent)))return RHIZO_ERROR_INVALID_ARGUMENT;
  try{*out=c->scene.create(name&&*name?name:"Object",parent);return *out?RHIZO_SUCCESS:RHIZO_ERROR_OUT_OF_MEMORY;}catch(...){return RHIZO_ERROR_OUT_OF_MEMORY;}
}
RhizoResult rhizo_scene_destroy_node(RhizoContext*c,uint32_t id){return c&&c->scene.destroy(id)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;}
RhizoResult rhizo_scene_set_parent(RhizoContext*c,uint32_t id,uint32_t parent){return c&&c->scene.set_parent(id,parent)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;}
RhizoResult rhizo_scene_set_transform(RhizoContext*c,uint32_t id,const RhizoTransform*t){
  if(!c||!t)return RHIZO_ERROR_INVALID_ARGUMENT;auto*n=c->scene.get(id);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;
  n->local.position={t->position[0],t->position[1],t->position[2]};
  n->local.rotation={t->rotation_xyzw[3],t->rotation_xyzw[0],t->rotation_xyzw[1],t->rotation_xyzw[2]};
  n->local.scale={t->scale[0],t->scale[1],t->scale[2]};return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_transform(const RhizoContext*c,uint32_t id,RhizoTransform*out){
  if(!c||!out)return RHIZO_ERROR_INVALID_ARGUMENT;const auto*n=c->scene.get(id);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;
  out->position[0]=n->local.position.x;out->position[1]=n->local.position.y;out->position[2]=n->local.position.z;
  out->rotation_xyzw[0]=n->local.rotation.x;out->rotation_xyzw[1]=n->local.rotation.y;out->rotation_xyzw[2]=n->local.rotation.z;out->rotation_xyzw[3]=n->local.rotation.w;
  out->scale[0]=n->local.scale.x;out->scale[1]=n->local.scale.y;out->scale[2]=n->local.scale.z;return RHIZO_SUCCESS;
}
uint32_t rhizo_scene_get_node_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->scene.nodes().size()):0;}
RhizoResult rhizo_scene_get_node(const RhizoContext*c,uint32_t index,RhizoSceneNodeInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoSceneNodeInfo)||index>=c->scene.nodes().size())return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto&n=c->scene.nodes()[index];out->entity_id=n.id;out->parent_id=n.parent;out->child_count=static_cast<uint32_t>(n.children.size());out->mesh_count=static_cast<uint32_t>(n.mesh_indices.size());out->visible=n.visible?1:0;
  std::memset(out->name,0,sizeof(out->name));std::strncpy(out->name,n.name.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_set_keyframe(RhizoContext*c,uint32_t id,uint32_t property,double time,const RhizoTransform*t){
  if(!c||!t||property>2)return RHIZO_ERROR_INVALID_ARGUMENT;rhizo::Transform v;
  v.position={t->position[0],t->position[1],t->position[2]};v.rotation={t->rotation_xyzw[3],t->rotation_xyzw[0],t->rotation_xyzw[1],t->rotation_xyzw[2]};v.scale={t->scale[0],t->scale[1],t->scale[2]};
  return c->animation.set_key(id,static_cast<rhizo::TrackProperty>(property),time,v)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_scene_evaluate_animation(RhizoContext*c,double time){return c&&c->animation.evaluate(c->scene,time)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;}
RhizoResult rhizo_scene_import_model(RhizoContext*c,const char*path,const char*cache,RhizoImportReport*out){
  if(!c||!path||!*path||!out||out->struct_size<sizeof(RhizoImportReport))return RHIZO_ERROR_INVALID_ARGUMENT;
  rhizo::ImportReport report;std::string error;
  if(!rhizo::import_model(c->scene,path,cache?cache:"",report,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}
  out->node_count=report.nodes;out->mesh_count=report.meshes;out->material_count=report.materials;
  std::memset(out->resolved_model_path,0,sizeof(out->resolved_model_path));std::strncpy(out->resolved_model_path,report.model_path.c_str(),sizeof(out->resolved_model_path)-1);return RHIZO_SUCCESS;
}
uint32_t rhizo_scene_get_mesh_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->scene.meshes.size()):0;}
RhizoResult rhizo_scene_get_mesh_info(const RhizoContext*c,uint32_t i,RhizoMeshInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoMeshInfo)||i>=c->scene.meshes.size())return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto&m=c->scene.meshes[i];out->vertex_count=static_cast<uint32_t>(m.vertices.size());out->index_count=static_cast<uint32_t>(m.indices.size());out->material_index=m.material_index;
  std::memset(out->name,0,sizeof(out->name));std::strncpy(out->name,m.name.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
uint32_t rhizo_scene_get_material_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->scene.materials.size()):0;}
RhizoResult rhizo_scene_get_material_info(const RhizoContext*c,uint32_t i,RhizoMaterialInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoMaterialInfo)||i>=c->scene.materials.size())return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto&m=c->scene.materials[i];out->base_color[0]=m.base_color.x;out->base_color[1]=m.base_color.y;out->base_color[2]=m.base_color.z;out->base_color[3]=m.base_color.w;out->metallic=m.metallic;out->roughness=m.roughness;
  auto cp=[](char*d,size_t n,const std::string&s){std::memset(d,0,n);std::strncpy(d,s.c_str(),n-1);};
  cp(out->base_color_texture,sizeof(out->base_color_texture),m.base_color_texture);cp(out->normal_texture,sizeof(out->normal_texture),m.normal_texture);cp(out->metallic_roughness_texture,sizeof(out->metallic_roughness_texture),m.metallic_roughness_texture);cp(out->emissive_texture,sizeof(out->emissive_texture),m.emissive_texture);return RHIZO_SUCCESS;
}


RhizoResult rhizo_shadow_map_create(RhizoContext* c,uint32_t width,uint32_t height,RhizoShadowMapInfo* out){
  if(!c||!out||out->struct_size<sizeof(RhizoShadowMapInfo))return RHIZO_ERROR_INVALID_ARGUMENT;
  try {
    auto map=std::make_unique<rhizo::ShadowMap>();std::string error;
    if(!map->create(c->vulkan,width,height,error)){c->last_error=std::move(error);return RHIZO_ERROR_VULKAN;}
    out->width=map->width();out->height=map->height();out->format=static_cast<uint32_t>(map->format());
    c->shadow_map=std::move(map);return RHIZO_SUCCESS;
  } catch (...) { c->last_error="Unable to allocate shadow map";return RHIZO_ERROR_OUT_OF_MEMORY; }
}
void rhizo_shadow_map_destroy(RhizoContext* c){if(c)c->shadow_map.reset();}
uint32_t rhizo_editor_grid_vertex_count(float extent,uint32_t subdivisions){
  return static_cast<uint32_t>(rhizo::make_grid(extent,subdivisions).vertices.size());
}
RhizoResult rhizo_editor_grid_vertices(float extent,uint32_t subdivisions,RhizoEditorVertex* out,uint32_t capacity){
  if(!out)return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto geometry=rhizo::make_grid(extent,subdivisions);
  if(capacity<geometry.vertices.size())return RHIZO_ERROR_INVALID_ARGUMENT;
  for(size_t i=0;i<geometry.vertices.size();++i){const auto&v=geometry.vertices[i];
    out[i].position[0]=v.position.x;out[i].position[1]=v.position.y;out[i].position[2]=v.position.z;
    out[i].color[0]=v.color.x;out[i].color[1]=v.color.y;out[i].color[2]=v.color.z;out[i].color[3]=v.color.w;}
  return RHIZO_SUCCESS;
}
uint32_t rhizo_editor_gizmo_vertex_count(void){return 18u;}
RhizoResult rhizo_editor_gizmo_vertices(const float origin[3],float axis_length,RhizoEditorVertex* out,uint32_t capacity){
  if(!origin||!out||capacity<18u)return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto geometry=rhizo::make_transform_gizmo({origin[0],origin[1],origin[2]},axis_length);
  if(geometry.vertices.size()!=18u)return RHIZO_ERROR_INVALID_ARGUMENT;
  for(size_t i=0;i<geometry.vertices.size();++i){const auto&v=geometry.vertices[i];
    out[i].position[0]=v.position.x;out[i].position[1]=v.position.y;out[i].position[2]=v.position.z;
    out[i].color[0]=v.color.x;out[i].color[1]=v.color.y;out[i].color[2]=v.color.z;out[i].color[3]=v.color.w;}
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_scene_set_camera(RhizoContext*c,uint32_t id,const RhizoCameraInfo*in){
  if(!c||!in||in->struct_size<sizeof(RhizoCameraInfo)||!std::isfinite(in->vertical_fov)||!std::isfinite(in->near_plane)||!std::isfinite(in->far_plane)||!std::isfinite(in->orthographic_size)||in->vertical_fov<=0.01f||in->vertical_fov>=3.13f||in->near_plane<=0||in->far_plane<=in->near_plane||in->orthographic_size<=0)return RHIZO_ERROR_INVALID_ARGUMENT;
  auto*n=c->scene.get(id);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;n->camera={in->vertical_fov,in->near_plane,in->far_plane,in->orthographic_size,in->orthographic!=0};n->has_camera=true;return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_camera(const RhizoContext*c,uint32_t id,RhizoCameraInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoCameraInfo))return RHIZO_ERROR_INVALID_ARGUMENT;const auto*n=c->scene.get(id);if(!n||!n->has_camera)return RHIZO_ERROR_INVALID_ARGUMENT;
  out->vertical_fov=n->camera.vertical_fov;out->near_plane=n->camera.near_plane;out->far_plane=n->camera.far_plane;out->orthographic_size=n->camera.orthographic_size;out->orthographic=n->camera.orthographic?1:0;return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_set_light(RhizoContext*c,uint32_t id,const RhizoLightInfo*in){
  if(!c||!in||in->struct_size<sizeof(RhizoLightInfo)||in->type>2||!std::isfinite(in->intensity)||!std::isfinite(in->range)||!std::isfinite(in->inner_cone)||!std::isfinite(in->outer_cone)||in->intensity<0||in->range<=0||in->inner_cone<0||in->outer_cone<=in->inner_cone||in->outer_cone>1.5707964f)return RHIZO_ERROR_INVALID_ARGUMENT;
  auto*n=c->scene.get(id);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;n->light.type=static_cast<rhizo::LightType>(in->type);n->light.color={in->color[0],in->color[1],in->color[2]};n->light.intensity=in->intensity;n->light.range=in->range;n->light.inner_cone=in->inner_cone;n->light.outer_cone=in->outer_cone;n->light.casts_shadows=in->casts_shadows!=0;n->has_light=true;return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_light(const RhizoContext*c,uint32_t id,RhizoLightInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoLightInfo))return RHIZO_ERROR_INVALID_ARGUMENT;const auto*n=c->scene.get(id);if(!n||!n->has_light)return RHIZO_ERROR_INVALID_ARGUMENT;
  out->type=static_cast<uint32_t>(n->light.type);out->color[0]=n->light.color.x;out->color[1]=n->light.color.y;out->color[2]=n->light.color.z;out->intensity=n->light.intensity;out->range=n->light.range;out->inner_cone=n->light.inner_cone;out->outer_cone=n->light.outer_cone;out->casts_shadows=n->light.casts_shadows?1:0;return RHIZO_SUCCESS;
}
}  // extern "C"
