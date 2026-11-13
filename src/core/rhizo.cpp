#include "rhizo/rhizo.h"

#include "vulkan/gpu_buffer.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/viewport.hpp"
#include "vulkan/frame_texture.hpp"
#include "rhizo/scene.hpp"
#include "rhizo/animation.hpp"
#include "rhizo/model_importer.hpp"
#include "rhizo/scene_io.hpp"
#include "rhizo/resource_manager.hpp"
#include "rhizo/shadow_map.hpp"
#include "rhizo/editor_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <map>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

struct RhizoBuffer;

struct RhizoContext {
  rhizo::VulkanContext vulkan;
  rhizo::Viewport viewport;
  std::string last_error;
  std::vector<RhizoBuffer*> buffers;
  rhizo::Scene scene;
  rhizo::AssetManager assets;
  rhizo::ResourceManager resources;
  rhizo::Animation animation;
  rhizo::AnimationPlayer animation_player;
  std::map<uint32_t,rhizo::Animation> animation_clips;
  std::map<uint32_t,std::string> animation_clip_names;
  uint32_t next_animation_clip_id=1;
  rhizo::AnimationStateMachine animation_state_machine;
  std::unordered_map<uint32_t,rhizo::Vec3> camera_shake_origins;
  uint32_t selected_entity=0,gizmo_mode=RHIZO_GIZMO_TRANSLATE;
  std::unique_ptr<rhizo::ShadowMap> shadow_map;
  RhizoRenderSettings render_settings{sizeof(RhizoRenderSettings),RHIZO_SHADOWS_MAPS,2048,0.0015f,0.01f,1.0f,1.0f,1.0f,1,1,1,1,{0,0,0,0}};
  std::vector<uint8_t> opengl_frame;
  RhizoOpenGLFrameInfo opengl_frame_info{sizeof(RhizoOpenGLFrameInfo),0,0,0,0,0,RHIZO_FRAME_RGBA8,0};
  std::unique_ptr<rhizo::GpuBuffer> opengl_frame_gpu;
  std::unique_ptr<rhizo::FrameTexture> opengl_frame_texture;
};

struct RhizoTexture { std::shared_ptr<const rhizo::TextureResource> resource; };

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

RhizoResult rhizo_viewport_attach(RhizoContext* context, void* native_window) {
  if (!context || !native_window) return RHIZO_ERROR_INVALID_ARGUMENT;
  context->viewport.set_render_settings(context->render_settings);
  std::string error;
  if (!context->viewport.attach(context->vulkan, static_cast<ANativeWindow*>(native_window), error)) { context->last_error = std::move(error); return RHIZO_ERROR_VULKAN; }
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_viewport_render(RhizoContext* context, float r, float g, float b, float a) {
  if (!context) return RHIZO_ERROR_INVALID_ARGUMENT;
  context->viewport.set_render_settings(context->render_settings);
  std::string error;
  if (!context->viewport.render(r,g,b,a,error)) { context->last_error = std::move(error); return RHIZO_ERROR_VULKAN; }
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_viewport_render_scene(RhizoContext* context,const float vp[16]) { if(!context||!vp)return RHIZO_ERROR_INVALID_ARGUMENT; rhizo::Mat4 matrix{}; std::copy(vp,vp+16,matrix.v.begin()); context->viewport.set_render_settings(context->render_settings); std::string error; if(!context->viewport.render_scene(context->scene,matrix,context->selected_entity,context->gizmo_mode,error,context->opengl_frame_texture.get(),context->opengl_frame_info.frame_index)){context->last_error=std::move(error);return RHIZO_ERROR_VULKAN;} return RHIZO_SUCCESS; }
RhizoResult rhizo_viewport_render_scene_camera(RhizoContext*c,uint32_t id){
 if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;const rhizo::SceneNode*n=c->scene.get(id);if(!n||!n->has_camera)return RHIZO_ERROR_INVALID_ARGUMENT;
 const rhizo::Mat4 world=c->scene.world_matrix(id);const rhizo::Vec3 pos{world.v[12],world.v[13],world.v[14]},forward=rhizo::normalize({-world.v[8],-world.v[9],-world.v[10]}),up=rhizo::normalize({world.v[4],world.v[5],world.v[6]});
 const rhizo::Mat4 view=rhizo::look_at(pos,pos+forward,up);const float aspect=static_cast<float>(c->viewport.width())/static_cast<float>(std::max(1u,c->viewport.height()));rhizo::Mat4 projection{};
 if(n->camera.orthographic){const float h=n->camera.orthographic_size,w=h*aspect;projection.v[0]=1.0f/w;projection.v[5]=-1.0f/h;projection.v[10]=1.0f/(n->camera.near_plane-n->camera.far_plane);projection.v[14]=n->camera.near_plane/(n->camera.near_plane-n->camera.far_plane);projection.v[15]=1.0f;}else projection=rhizo::perspective(n->camera.vertical_fov,aspect,n->camera.near_plane,n->camera.far_plane);
 c->viewport.set_render_settings(c->render_settings);std::string error;if(!c->viewport.render_scene(c->scene,rhizo::multiply(projection,view),c->selected_entity,c->gizmo_mode,error,c->opengl_frame_texture.get(),c->opengl_frame_info.frame_index,&pos)){c->last_error=std::move(error);return RHIZO_ERROR_VULKAN;}return RHIZO_SUCCESS;
}
void rhizo_viewport_detach(RhizoContext* context) { if (context) context->viewport.detach(); }

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



RhizoResult rhizo_render_settings_set(RhizoContext*c,const RhizoRenderSettings*in){
 if(!c||!in||in->struct_size<sizeof(RhizoRenderSettings)||in->shadow_mode>RHIZO_SHADOWS_MAPS||
 (in->shadow_resolution!=512&&in->shadow_resolution!=1024&&in->shadow_resolution!=2048&&in->shadow_resolution!=4096&&in->shadow_resolution!=8192)||
 !std::isfinite(in->shadow_bias)||!std::isfinite(in->shadow_normal_bias)||!std::isfinite(in->exposure)||!std::isfinite(in->environment_intensity)||!std::isfinite(in->render_scale)||
 in->shadow_bias<0||in->shadow_normal_bias<0||in->exposure<0||in->environment_intensity<0||in->render_scale<0.25f||in->render_scale>2.0f)return RHIZO_ERROR_INVALID_ARGUMENT;
 c->render_settings=*in;return RHIZO_SUCCESS;
}
RhizoResult rhizo_render_settings_get(const RhizoContext*c,RhizoRenderSettings*out){
 if(!c||!out||out->struct_size<sizeof(RhizoRenderSettings))return RHIZO_ERROR_INVALID_ARGUMENT;
 *out=c->render_settings;return RHIZO_SUCCESS;
}
RhizoResult rhizo_opengl_submit_frame(RhizoContext*c,const RhizoOpenGLFrameInfo*i,const void*p,uint64_t bytes){
 if(!c||!i||!p||i->struct_size<sizeof(RhizoOpenGLFrameInfo)||!i->width||!i->height||i->width>16384||i->height>16384||
 (i->pixel_format!=RHIZO_FRAME_RGBA8&&i->pixel_format!=RHIZO_FRAME_BGRA8))return RHIZO_ERROR_INVALID_ARGUMENT;
 const uint64_t row=uint64_t(i->width)*4u;
 if(i->stride_bytes<row||(i->stride_bytes%4u)!=0||uint64_t(i->stride_bytes)>uint64_t(i->width)*16u||uint64_t(i->stride_bytes)>UINT64_MAX/i->height||
 bytes<uint64_t(i->stride_bytes)*i->height||bytes>512ull*1024ull*1024ull||bytes>SIZE_MAX)return RHIZO_ERROR_INVALID_ARGUMENT;
 try{
  std::vector<uint8_t> staging(static_cast<size_t>(bytes));std::memcpy(staging.data(),p,static_cast<size_t>(bytes));
  auto gpu=std::make_unique<rhizo::GpuBuffer>();std::string error;
  if(!gpu->create(c->vulkan,static_cast<VkDeviceSize>(bytes),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true,error)||
     !gpu->write(0,staging.data(),static_cast<VkDeviceSize>(bytes),error)){c->last_error=std::move(error);return RHIZO_ERROR_VULKAN;}
  auto texture=std::make_unique<rhizo::FrameTexture>();
  if(!texture->upload(c->vulkan,gpu->handle(),i->width,i->height,i->stride_bytes,i->pixel_format==RHIZO_FRAME_BGRA8,error,true)){c->last_error=std::move(error);return RHIZO_ERROR_VULKAN;}
  c->opengl_frame.swap(staging);c->opengl_frame_info=*i;c->opengl_frame_gpu=std::move(gpu);c->opengl_frame_texture=std::move(texture);return RHIZO_SUCCESS;
 }catch(...){c->last_error="OpenGL frame upload allocation failed";return RHIZO_ERROR_OUT_OF_MEMORY;}
}
RhizoResult rhizo_opengl_get_latest_texture(const RhizoContext*c,uint64_t*view,uint64_t*sampler,uint32_t*w,uint32_t*h){
 if(!c||!view||!sampler||!w||!h||!c->opengl_frame_texture)return RHIZO_ERROR_INVALID_ARGUMENT;
 *view=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c->opengl_frame_texture->view()));
 *sampler=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c->opengl_frame_texture->sampler()));
 *w=c->opengl_frame_texture->width();*h=c->opengl_frame_texture->height();return RHIZO_SUCCESS;
}
RhizoResult rhizo_opengl_get_latest_frame(const RhizoContext*c,RhizoOpenGLFrameInfo*i,const void**p,uint64_t*n){
 if(!c||!i||!p||!n||i->struct_size<sizeof(RhizoOpenGLFrameInfo)||c->opengl_frame.empty())return RHIZO_ERROR_INVALID_ARGUMENT;
 *i=c->opengl_frame_info;*p=c->opengl_frame.data();*n=c->opengl_frame.size();return RHIZO_SUCCESS;
}


RhizoResult rhizo_editor_set_selection(RhizoContext*c,uint32_t id){
 if(!c||(id!=0&&!c->scene.get(id)))return RHIZO_ERROR_INVALID_ARGUMENT;c->selected_entity=id;return RHIZO_SUCCESS;
}
uint32_t rhizo_editor_get_selection(const RhizoContext*c){return c?c->selected_entity:0;}
RhizoResult rhizo_editor_set_gizmo_mode(RhizoContext*c,uint32_t mode){
 if(!c||mode>RHIZO_GIZMO_SCALE)return RHIZO_ERROR_INVALID_ARGUMENT;c->gizmo_mode=mode;return RHIZO_SUCCESS;
}
uint32_t rhizo_editor_get_gizmo_mode(const RhizoContext*c){return c?c->gizmo_mode:RHIZO_GIZMO_TRANSLATE;}
namespace {
bool project_gizmo_point(const float vp[16],uint32_t w,uint32_t h,rhizo::Vec3 p,rhizo::Vec2&out){
 const float x=vp[0]*p.x+vp[4]*p.y+vp[8]*p.z+vp[12],y=vp[1]*p.x+vp[5]*p.y+vp[9]*p.z+vp[13],z=vp[2]*p.x+vp[6]*p.y+vp[10]*p.z+vp[14],q=vp[3]*p.x+vp[7]*p.y+vp[11]*p.z+vp[15];
 if(q<=1.0e-5f||!std::isfinite(q))return false;out={(x/q*0.5f+0.5f)*static_cast<float>(w),(1.0f-(y/q*0.5f+0.5f))*static_cast<float>(h)};return std::isfinite(out.x)&&std::isfinite(out.y)&&z/q>=0.0f;
}
void gizmo_axis(uint32_t axis,rhizo::Vec3&out){out=axis==0?rhizo::Vec3{1,0,0}:(axis==1?rhizo::Vec3{0,1,0}:rhizo::Vec3{0,0,1});}
void rotate_axis(rhizo::Vec3 a,const rhizo::Quat&q,rhizo::Vec3&out){rhizo::Vec3 v{q.x,q.y,q.z};rhizo::Vec3 t=rhizo::cross(v,a)*2.0f;out=a+t*q.w+rhizo::cross(v,t);}
float point_segment_distance(rhizo::Vec2 p,rhizo::Vec2 a,rhizo::Vec2 b){float dx=b.x-a.x,dy=b.y-a.y,l=dx*dx+dy*dy,t=l>1e-6f?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/l,0.0f,1.0f):0.0f;float x=a.x+t*dx,y=a.y+t*dy;return std::hypot(p.x-x,p.y-y);}
}
RhizoResult rhizo_editor_gizmo_pick_axis(const float vp[16],uint32_t w,uint32_t h,const float origin[3],float size,float cx,float cy,float radius,uint32_t*out){
 if(!vp||!origin||!out||!w||!h||!std::isfinite(size)||size<=0||!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(radius)||radius<=0)return RHIZO_ERROR_INVALID_ARGUMENT;
 rhizo::Vec3 o{origin[0],origin[1],origin[2]};rhizo::Vec2 p{cx,cy},a,b;if(!project_gizmo_point(vp,w,h,o,a))return RHIZO_ERROR_INVALID_ARGUMENT;
 float best=radius;uint32_t selected=UINT32_MAX;
 for(uint32_t i=0;i<3;i++){rhizo::Vec3 axis{};gizmo_axis(i,axis);if(!project_gizmo_point(vp,w,h,o+axis*size,b))continue;float d=point_segment_distance(p,a,b);if(d<best){best=d;selected=i;}}
 if(selected==UINT32_MAX)return RHIZO_ERROR_INVALID_ARGUMENT;*out=selected;return RHIZO_SUCCESS;
}
RhizoResult rhizo_editor_gizmo_drag(RhizoContext*c,const float vp[16],uint32_t w,uint32_t h,uint32_t axis,float size,float cx,float cy,float dx,float dy,uint8_t local){
 if(!c||!vp||!w||!h||axis>2||!std::isfinite(size)||size<=0||!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(dx)||!std::isfinite(dy)||!c->selected_entity)return RHIZO_ERROR_INVALID_ARGUMENT;
 auto*n=c->scene.get(c->selected_entity);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;
 rhizo::Mat4 world=c->scene.world_matrix(n->id);rhizo::Vec3 origin{world.v[12],world.v[13],world.v[14]},a{};gizmo_axis(axis,a);
 if(local)rotate_axis(a,n->local.rotation,a);
 rhizo::Vec2 center,end;if(!project_gizmo_point(vp,w,h,origin,center))return RHIZO_ERROR_INVALID_ARGUMENT;
 float amount=0;
 if(c->gizmo_mode==RHIZO_GIZMO_ROTATE){
   rhizo::Vec3 u=axis==0?rhizo::Vec3{0,1,0}:(axis==1?rhizo::Vec3{1,0,0}:rhizo::Vec3{1,0,0});
   rotate_axis(u,n->local.rotation,u);
   rhizo::Vec2 edge;if(!project_gizmo_point(vp,w,h,origin+u*size,edge))return RHIZO_ERROR_INVALID_ARGUMENT;
   float rx=cx-center.x,ry=cy-center.y,r=std::hypot(rx,ry);if(r<4.0f)return RHIZO_ERROR_INVALID_ARGUMENT;
   float tx=-ry/r,ty=rx/r;amount=(dx*tx+dy*ty)/r;
 }else{
   if(!project_gizmo_point(vp,w,h,origin+a*size,end))return RHIZO_ERROR_INVALID_ARGUMENT;
   float ax=end.x-center.x,ay=end.y-center.y,l=ax*ax+ay*ay;if(l<1.0f)return RHIZO_ERROR_INVALID_ARGUMENT;
   amount=((dx*ax+dy*ay)/l);
   if(c->gizmo_mode==RHIZO_GIZMO_TRANSLATE)amount*=size;
 }
 return rhizo_editor_apply_gizmo_delta(c,axis,amount,local);
}
RhizoResult rhizo_editor_apply_gizmo_delta(RhizoContext*c,uint32_t axis,float delta,uint8_t local){
 if(!c||axis>2||!std::isfinite(delta)||!c->selected_entity)return RHIZO_ERROR_INVALID_ARGUMENT;
 auto*n=c->scene.get(c->selected_entity);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;
 rhizo::Vec3 a{axis==0?1.0f:0.0f,axis==1?1.0f:0.0f,axis==2?1.0f:0.0f};
 if(local){const auto q=n->local.rotation;const rhizo::Vec3 v{q.x,q.y,q.z};const rhizo::Vec3 t=rhizo::cross(v,a)*2.0f;a=a+ t*q.w+rhizo::cross(v,t);}
 if(c->gizmo_mode==RHIZO_GIZMO_TRANSLATE)n->local.position=n->local.position+a*delta;
 else if(c->gizmo_mode==RHIZO_GIZMO_SCALE){const float factor=std::max(0.001f,1.0f+delta);if(axis==0)n->local.scale.x*=factor;else if(axis==1)n->local.scale.y*=factor;else n->local.scale.z*=factor;}
 else {const float half=delta*0.5f,s=std::sin(half),co=std::cos(half);rhizo::Quat d{a.x*s,a.y*s,a.z*s,co};const auto q=n->local.rotation;
 n->local.rotation={d.w*q.x+d.x*q.w+d.y*q.z-d.z*q.y,d.w*q.y-d.x*q.z+d.y*q.w+d.z*q.x,d.w*q.z+d.x*q.y-d.y*q.x+d.z*q.w,d.w*q.w-d.x*q.x-d.y*q.y-d.z*q.z};}
 return RHIZO_SUCCESS;
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
  std::string error;auto resource=c->resources.load_model(path,cache?cache:"",error);
  if(!resource){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}
  const rhizo::Scene&source=resource->scene;const uint32_t meshBase=static_cast<uint32_t>(c->scene.meshes.size()),materialBase=static_cast<uint32_t>(c->scene.materials.size());
  for(const auto&m:source.materials)c->scene.materials.push_back(m);
  for(auto m:source.meshes){if(m.material_index<source.materials.size())m.material_index+=materialBase;c->scene.meshes.push_back(std::move(m));}
  std::unordered_map<uint32_t,uint32_t> ids;for(const auto&n:source.nodes()){uint32_t id=c->scene.create(n.name);if(!id){c->last_error="Failed to instantiate cached model scene";return RHIZO_ERROR_INTERNAL;}ids[n.id]=id;}
  for(const auto&n:source.nodes()){auto*dst=c->scene.get(ids[n.id]);dst->local=n.local;dst->visible=n.visible;dst->has_camera=n.has_camera;dst->camera=n.camera;dst->has_light=n.has_light;dst->light=n.light;dst->morph_weights=n.morph_weights;for(uint32_t m:n.mesh_indices)dst->mesh_indices.push_back(meshBase+m);if(n.parent&&!c->scene.set_parent(dst->id,ids[n.parent])){c->last_error="Cached model hierarchy is invalid";return RHIZO_ERROR_INTERNAL;}}
  for(size_t mi=0;mi<source.meshes.size();mi++)for(auto&bone:c->scene.meshes[meshBase+mi].bones){auto mapped=ids.find(bone.node);bone.node=mapped==ids.end()?0:mapped->second;}
  for(const auto&clip:resource->animations){rhizo::Animation remapped;if(!rhizo::retarget_animation(clip.animation,ids,remapped))continue;while(c->animation_clips.contains(c->next_animation_clip_id))++c->next_animation_clip_id;const uint32_t clipId=c->next_animation_clip_id++;c->animation_clips.emplace(clipId,std::move(remapped));c->animation_clip_names[clipId]=clip.name;}
  out->node_count=static_cast<uint32_t>(source.nodes().size());out->mesh_count=static_cast<uint32_t>(source.meshes.size());out->material_count=static_cast<uint32_t>(source.materials.size());
  std::memset(out->resolved_model_path,0,sizeof(out->resolved_model_path));std::strncpy(out->resolved_model_path,resource->source.c_str(),sizeof(out->resolved_model_path)-1);c->last_error.clear();return RHIZO_SUCCESS;
}
uint32_t rhizo_scene_get_mesh_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->scene.meshes.size()):0;}
RhizoResult rhizo_scene_get_mesh_info(const RhizoContext*c,uint32_t i,RhizoMeshInfo*out){
  if(!c||!out||out->struct_size<sizeof(RhizoMeshInfo)||i>=c->scene.meshes.size())return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto&m=c->scene.meshes[i];out->vertex_count=static_cast<uint32_t>(m.vertices.size());out->index_count=static_cast<uint32_t>(m.indices.size());out->material_index=m.material_index;
  std::memset(out->name,0,sizeof(out->name));std::strncpy(out->name,m.name.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
uint32_t rhizo_scene_get_material_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->scene.materials.size()):0;}
RhizoResult rhizo_scene_get_mesh_skin_info(const RhizoContext*c,uint32_t mesh,RhizoMeshSkinInfo*out){
 if(!c||!out||out->struct_size<sizeof(RhizoMeshSkinInfo)||mesh>=c->scene.meshes.size())return RHIZO_ERROR_INVALID_ARGUMENT;const auto&m=c->scene.meshes[mesh];out->bone_count=static_cast<uint32_t>(m.bones.size());out->morph_target_count=static_cast<uint32_t>(m.morph_targets.size());return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_mesh_bone_info(const RhizoContext*c,uint32_t mesh,uint32_t bone,RhizoBoneInfo*out){
 if(!c||!out||out->struct_size<sizeof(RhizoBoneInfo)||mesh>=c->scene.meshes.size()||bone>=c->scene.meshes[mesh].bones.size())return RHIZO_ERROR_INVALID_ARGUMENT;const auto&b=c->scene.meshes[mesh].bones[bone];out->bone_index=bone;out->node_id=b.node;std::memset(out->name,0,sizeof(out->name));std::strncpy(out->name,b.name.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_mesh_morph_info(const RhizoContext*c,uint32_t mesh,uint32_t target,RhizoMorphTargetInfo*out){
 if(!c||!out||out->struct_size<sizeof(RhizoMorphTargetInfo)||mesh>=c->scene.meshes.size()||target>=c->scene.meshes[mesh].morph_targets.size())return RHIZO_ERROR_INVALID_ARGUMENT;out->target_index=target;std::memset(out->name,0,sizeof(out->name));std::strncpy(out->name,c->scene.meshes[mesh].morph_targets[target].name.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_set_morph_weight(RhizoContext*c,uint32_t entity,uint32_t target,float weight){
 if(!c||!std::isfinite(weight)||weight<-1.0f||weight>1.0f)return RHIZO_ERROR_INVALID_ARGUMENT;auto*n=c->scene.get(entity);if(!n)return RHIZO_ERROR_INVALID_ARGUMENT;size_t maxTargets=0;for(uint32_t mesh:n->mesh_indices)if(mesh<c->scene.meshes.size())maxTargets=std::max(maxTargets,c->scene.meshes[mesh].morph_targets.size());if(target>=maxTargets||target>=32)return RHIZO_ERROR_INVALID_ARGUMENT;if(n->morph_weights.size()<=target)n->morph_weights.resize(target+1,0.0f);n->morph_weights[target]=weight;return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_get_morph_weight(const RhizoContext*c,uint32_t entity,uint32_t target,float*out){
 if(!c||!out)return RHIZO_ERROR_INVALID_ARGUMENT;const auto*n=c->scene.get(entity);if(!n||target>=n->morph_weights.size())return RHIZO_ERROR_INVALID_ARGUMENT;*out=n->morph_weights[target];return RHIZO_SUCCESS;
}

RhizoResult rhizo_scene_get_material_info(const RhizoContext*c,uint32_t i,RhizoMaterialInfo*out){
  constexpr size_t legacySize=offsetof(RhizoMaterialInfo,normal_scale);
  if(!c||!out||out->struct_size<legacySize||i>=c->scene.materials.size())return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto&m=c->scene.materials[i];out->base_color[0]=m.base_color.x;out->base_color[1]=m.base_color.y;out->base_color[2]=m.base_color.z;out->base_color[3]=m.base_color.w;out->metallic=m.metallic;out->roughness=m.roughness;
  if(out->struct_size>=sizeof(RhizoMaterialInfo)){out->normal_scale=m.normal_scale;out->occlusion=m.occlusion;out->emissive_color[0]=m.emissive_color.x;out->emissive_color[1]=m.emissive_color.y;out->emissive_color[2]=m.emissive_color.z;}
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

RhizoResult rhizo_scene_set_parameter_keyframe(RhizoContext*c,uint32_t id,uint32_t property,double time,const float value[4]){
  if(!c||!value)return RHIZO_ERROR_INVALID_ARGUMENT;
  return c->animation.set_parameter_key(id,property,time,{value[0],value[1],value[2],value[3]})?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}

RhizoResult rhizo_scene_remove_keyframe(RhizoContext*c,uint32_t id,uint32_t property,double time){
 if(!c||!std::isfinite(time)||time<0)return RHIZO_ERROR_INVALID_ARGUMENT;
 if(property<=2)return c->animation.remove_key(id,static_cast<rhizo::TrackProperty>(property),time)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
 return c->animation.remove_parameter_key(id,property,time)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_scene_set_keyframe_interpolation(RhizoContext*c,uint32_t id,uint32_t property,uint32_t mode){
 if(!c||property>2||mode>3)return RHIZO_ERROR_INVALID_ARGUMENT;
 return c->animation.set_interpolation(id,static_cast<rhizo::TrackProperty>(property),static_cast<rhizo::Interpolation>(mode))?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_scene_set_parameter_interpolation(RhizoContext*c,uint32_t id,uint32_t property,uint32_t mode){
 if(!c||mode>3)return RHIZO_ERROR_INVALID_ARGUMENT;
 return c->animation.set_parameter_interpolation(id,property,static_cast<rhizo::Interpolation>(mode))?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_play(RhizoContext*c,uint8_t restart){
 if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.bind(&c->animation);c->animation_player.play(restart!=0);return RHIZO_SUCCESS;
}
RhizoResult rhizo_animation_pause(RhizoContext*c){if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.pause();return RHIZO_SUCCESS;}
RhizoResult rhizo_animation_stop(RhizoContext*c){if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.stop();return RHIZO_SUCCESS;}
RhizoResult rhizo_animation_seek(RhizoContext*c,double time){if(!c||!std::isfinite(time)||time<0)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.bind(&c->animation);c->animation_player.seek(time);return c->animation_player.evaluate(c->scene)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;}
RhizoResult rhizo_animation_set_speed(RhizoContext*c,double speed){if(!c||!std::isfinite(speed)||speed==0||std::abs(speed)>8)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.set_speed(speed);return RHIZO_SUCCESS;}
RhizoResult rhizo_animation_set_mode(RhizoContext*c,uint32_t mode){if(!c||mode>2)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.set_mode(static_cast<rhizo::PlaybackMode>(mode));return RHIZO_SUCCESS;}
RhizoResult rhizo_animation_tick(RhizoContext*c,double delta){if(!c||!std::isfinite(delta)||delta<0||delta>1.0)return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.bind(&c->animation);return c->animation_player.tick(c->scene,delta)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;}
double rhizo_animation_get_time(const RhizoContext*c){return c?c->animation_player.time():0.0;}
uint32_t rhizo_animation_get_key_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->animation.key_count()):0;}



RhizoResult rhizo_animation_clip_create(RhizoContext*c,uint32_t clip){
 if(!c||clip==0||c->animation_clips.contains(clip))return RHIZO_ERROR_INVALID_ARGUMENT;try{c->animation_clips.emplace(clip,rhizo::Animation{});c->animation_clip_names[clip]=std::string("Clip_")+std::to_string(clip);c->next_animation_clip_id=std::max(c->next_animation_clip_id,clip+1u);return RHIZO_SUCCESS;}catch(...){return RHIZO_ERROR_OUT_OF_MEMORY;}
}
RhizoResult rhizo_animation_clip_remove(RhizoContext*c,uint32_t clip){
 if(!c||!c->animation_clips.contains(clip))return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_clips.erase(clip);c->animation_clip_names.erase(clip);c->animation_player.bind(nullptr);c->animation_state_machine=rhizo::AnimationStateMachine{};return RHIZO_SUCCESS;
}
uint32_t rhizo_animation_clip_get_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->animation_clips.size()):0;}
RhizoResult rhizo_animation_clip_get_info(const RhizoContext*c,uint32_t index,RhizoAnimationClipInfo*out){
 if(!c||!out||out->struct_size<sizeof(RhizoAnimationClipInfo)||index>=c->animation_clips.size())return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.begin();std::advance(it,index);out->clip_id=it->first;out->key_count=static_cast<uint32_t>(it->second.key_count());out->duration=it->second.duration();std::memset(out->name,0,sizeof(out->name));auto name=c->animation_clip_names.find(it->first);const std::string label=name==c->animation_clip_names.end()?std::string("Clip_")+std::to_string(it->first):name->second;std::strncpy(out->name,label.c_str(),sizeof(out->name)-1);return RHIZO_SUCCESS;
}
RhizoResult rhizo_animation_clip_set_interpolation(RhizoContext*c,uint32_t clip,uint32_t id,uint32_t property,uint32_t mode){
 if(!c||mode>3)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;const auto interpolation=static_cast<rhizo::Interpolation>(mode);if(property<3)return it->second.set_interpolation(id,static_cast<rhizo::TrackProperty>(property),interpolation)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;return it->second.set_parameter_interpolation(id,property,interpolation)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_clip_remove_keyframe(RhizoContext*c,uint32_t clip,uint32_t id,uint32_t property,double time){
 if(!c||!std::isfinite(time))return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;const bool removed=property<3?it->second.remove_key(id,static_cast<rhizo::TrackProperty>(property),time):it->second.remove_parameter_key(id,property,time);return removed?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_play_clip(RhizoContext*c,uint32_t clip,uint8_t restart){
 if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;c->animation_player.bind(&it->second);c->animation_player.play(restart!=0);return RHIZO_SUCCESS;
}
RhizoResult rhizo_animation_clip_retarget(RhizoContext*c,uint32_t source,uint32_t destination,const uint32_t*from,const uint32_t*to,uint32_t count){
 if(!c||!from||!to||count==0||count>100000||source==destination)return RHIZO_ERROR_INVALID_ARGUMENT;auto src=c->animation_clips.find(source),dst=c->animation_clips.find(destination);if(src==c->animation_clips.end()||dst==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;std::unordered_map<uint32_t,uint32_t>mapping;for(uint32_t i=0;i<count;i++){if(!from[i]||!to[i])return RHIZO_ERROR_INVALID_ARGUMENT;mapping[from[i]]=to[i];}return rhizo::retarget_animation(src->second,mapping,dst->second)?RHIZO_SUCCESS:RHIZO_ERROR_INTERNAL;
}
RhizoResult rhizo_animation_clip_set_keyframe(RhizoContext*c,uint32_t clip,uint32_t id,uint32_t property,double time,const RhizoTransform*t){
 if(!c||!t||property>2)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;rhizo::Transform v;v.position={t->position[0],t->position[1],t->position[2]};v.rotation={t->rotation_xyzw[3],t->rotation_xyzw[0],t->rotation_xyzw[1],t->rotation_xyzw[2]};v.scale={t->scale[0],t->scale[1],t->scale[2]};return it->second.set_key(id,static_cast<rhizo::TrackProperty>(property),time,v)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_clip_set_parameter_keyframe(RhizoContext*c,uint32_t clip,uint32_t id,uint32_t property,double time,const float value[4]){
 if(!c||!value)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;return it->second.set_parameter_key(id,property,time,{value[0],value[1],value[2],value[3]})?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_state_add(RhizoContext*c,uint32_t state,uint32_t clip,double speed){
 if(!c||state==0||!std::isfinite(speed)||speed==0)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->animation_clips.find(clip);if(it==c->animation_clips.end())return RHIZO_ERROR_INVALID_ARGUMENT;return c->animation_state_machine.add_state(std::to_string(state),&it->second,speed)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_state_transition(RhizoContext*c,uint32_t from,uint32_t to,double duration){
 if(!c||from==0||to==0)return RHIZO_ERROR_INVALID_ARGUMENT;return c->animation_state_machine.set_transition(std::to_string(from),std::to_string(to),duration)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_state_play(RhizoContext*c,uint32_t state,uint8_t restart){
 if(!c||state==0)return RHIZO_ERROR_INVALID_ARGUMENT;return c->animation_state_machine.play(std::to_string(state),restart!=0)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_animation_state_tick(RhizoContext*c,double delta){
 if(!c||!std::isfinite(delta)||delta<0||delta>1.0)return RHIZO_ERROR_INVALID_ARGUMENT;return c->animation_state_machine.update(c->scene,delta)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}

RhizoResult rhizo_scene_look_at(RhizoContext*c,uint32_t entity,uint32_t target,float weight){
 if(!c||!std::isfinite(weight))return RHIZO_ERROR_INVALID_ARGUMENT;return rhizo::solve_look_at(c->scene,entity,target,weight)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_scene_solve_two_bone_ik(RhizoContext*c,uint32_t root,uint32_t middle,uint32_t end,const float target[3],const float pole[3],float weight){
 if(!c||!target||!pole||!std::isfinite(weight))return RHIZO_ERROR_INVALID_ARGUMENT;for(float v:{target[0],target[1],target[2],pole[0],pole[1],pole[2]})if(!std::isfinite(v))return RHIZO_ERROR_INVALID_ARGUMENT;
 return rhizo::solve_two_bone_ik(c->scene,root,middle,end,{target[0],target[1],target[2]},{pole[0],pole[1],pole[2]},weight)?RHIZO_SUCCESS:RHIZO_ERROR_INVALID_ARGUMENT;
}
RhizoResult rhizo_camera_shake(RhizoContext*c,uint32_t entity,double time,float amplitude,float frequency){
 if(!c||!std::isfinite(time)||time<0||!std::isfinite(amplitude)||!std::isfinite(frequency)||amplitude<0||amplitude>10||frequency<0||frequency>100)return RHIZO_ERROR_INVALID_ARGUMENT;
 auto*n=c->scene.get(entity);if(!n||!n->has_camera)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->camera_shake_origins.find(entity);if(it==c->camera_shake_origins.end())it=c->camera_shake_origins.emplace(entity,n->local.position).first;const double phase=time*static_cast<double>(frequency)*6.283185307179586;const float a=amplitude;n->local.position=it->second+rhizo::Vec3{static_cast<float>(std::sin(phase*1.13))*a,static_cast<float>(std::sin(phase*1.71+1.2))*a*0.65f,static_cast<float>(std::cos(phase*0.83+0.4))*a*0.45f};return RHIZO_SUCCESS;
}
RhizoResult rhizo_camera_shake_clear(RhizoContext*c,uint32_t entity){
 if(!c)return RHIZO_ERROR_INVALID_ARGUMENT;auto it=c->camera_shake_origins.find(entity);auto*n=c->scene.get(entity);if(it==c->camera_shake_origins.end()||!n)return RHIZO_ERROR_INVALID_ARGUMENT;n->local.position=it->second;c->camera_shake_origins.erase(it);return RHIZO_SUCCESS;
}

RhizoResult rhizo_scene_set_material_info(RhizoContext*c,uint32_t i,const RhizoMaterialInfo*in){
 constexpr size_t legacySize=offsetof(RhizoMaterialInfo,normal_scale);
 if(!c||!in||in->struct_size<legacySize||i>=c->scene.materials.size())return RHIZO_ERROR_INVALID_ARGUMENT;
 const bool hasExtended=in->struct_size>=sizeof(RhizoMaterialInfo);const float normalScale=hasExtended?in->normal_scale:1.0f,occlusion=hasExtended?in->occlusion:1.0f;const float emissive[3]={hasExtended?in->emissive_color[0]:0.0f,hasExtended?in->emissive_color[1]:0.0f,hasExtended?in->emissive_color[2]:0.0f};
 const float values[]={in->base_color[0],in->base_color[1],in->base_color[2],in->base_color[3],in->metallic,in->roughness,normalScale,occlusion,emissive[0],emissive[1],emissive[2]};for(float v:values)if(!std::isfinite(v))return RHIZO_ERROR_INVALID_ARGUMENT;
 if(in->metallic<0||in->metallic>1||in->roughness<0||in->roughness>1||normalScale<0||normalScale>16||occlusion<0||occlusion>1)return RHIZO_ERROR_INVALID_ARGUMENT;
 for(float v:in->base_color)if(v<0||v>1)return RHIZO_ERROR_INVALID_ARGUMENT;for(float v:emissive)if(v<0)return RHIZO_ERROR_INVALID_ARGUMENT;
 auto read=[](const char*s,size_t n,std::string&out){const void*end=std::memchr(s,0,n);if(!end)return false;out.assign(s,static_cast<size_t>(static_cast<const char*>(end)-s));return true;};
 std::string paths[4];if(!read(in->base_color_texture,sizeof(in->base_color_texture),paths[0])||!read(in->normal_texture,sizeof(in->normal_texture),paths[1])||!read(in->metallic_roughness_texture,sizeof(in->metallic_roughness_texture),paths[2])||!read(in->emissive_texture,sizeof(in->emissive_texture),paths[3]))return RHIZO_ERROR_INVALID_ARGUMENT;
 auto&m=c->scene.materials[i];m.base_color={in->base_color[0],in->base_color[1],in->base_color[2],in->base_color[3]};m.metallic=in->metallic;m.roughness=in->roughness;m.normal_scale=normalScale;m.occlusion=occlusion;m.emissive_color={emissive[0],emissive[1],emissive[2]};m.base_color_texture=std::move(paths[0]);m.normal_texture=std::move(paths[1]);m.metallic_roughness_texture=std::move(paths[2]);m.emissive_texture=std::move(paths[3]);return RHIZO_SUCCESS;
}

RhizoResult rhizo_scene_save(RhizoContext*c,const char*path){
 if(!c||!path||!*path)return RHIZO_ERROR_INVALID_ARGUMENT;std::string error;if(!rhizo::save_scene(c->scene,path,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}c->last_error.clear();return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_load(RhizoContext*c,const char*path){
 if(!c||!path||!*path)return RHIZO_ERROR_INVALID_ARGUMENT;rhizo::Scene loaded;std::string error;if(!rhizo::load_scene(loaded,path,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}
 c->scene=std::move(loaded);c->animation=rhizo::Animation{};c->animation_player.bind(nullptr);c->animation_clips.clear();c->animation_state_machine=rhizo::AnimationStateMachine{};c->camera_shake_origins.clear();c->selected_entity=0;c->last_error.clear();return RHIZO_SUCCESS;
}
RhizoResult rhizo_prefab_save(RhizoContext*c,uint32_t root,const char*path){
 if(!c||!path||!*path||!root)return RHIZO_ERROR_INVALID_ARGUMENT;std::string error;if(!rhizo::save_prefab(c->scene,root,path,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}c->last_error.clear();return RHIZO_SUCCESS;
}
RhizoResult rhizo_prefab_instantiate(RhizoContext*c,const char*path,uint32_t parent,uint32_t*out_root){
 if(!c||!path||!*path||!out_root)return RHIZO_ERROR_INVALID_ARGUMENT;*out_root=0;std::string error;if(!rhizo::instantiate_prefab(c->scene,path,parent,*out_root,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}c->last_error.clear();return RHIZO_SUCCESS;
}
RhizoResult rhizo_assets_scan(RhizoContext*c,const char*root){
 if(!c||!root||!*root)return RHIZO_ERROR_INVALID_ARGUMENT;std::string error;if(!c->assets.scan(root,error)){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}c->last_error.clear();return RHIZO_SUCCESS;
}
uint32_t rhizo_assets_get_count(const RhizoContext*c){return c?static_cast<uint32_t>(c->assets.assets().size()):0;}
RhizoResult rhizo_assets_get(const RhizoContext*c,uint32_t index,RhizoAssetInfo*out){
 if(!c||!out||out->struct_size<sizeof(RhizoAssetInfo)||index>=c->assets.assets().size())return RHIZO_ERROR_INVALID_ARGUMENT;const auto&a=c->assets.assets()[index];out->type=static_cast<uint32_t>(a.type);out->id=a.id;out->size=a.size;out->modified=a.modified;std::strncpy(out->path,a.path.c_str(),sizeof(out->path)-1);out->path[sizeof(out->path)-1]='\0';return RHIZO_SUCCESS;
}
void rhizo_resources_collect(RhizoContext*c){if(c)c->resources.collect_unused();}
void rhizo_resources_clear(RhizoContext*c){if(c)c->resources.clear();}
RhizoResult rhizo_resources_get_stats(const RhizoContext*c,RhizoResourceStats*out){
 if(!c||!out||out->struct_size<sizeof(RhizoResourceStats))return RHIZO_ERROR_INVALID_ARGUMENT;out->texture_cache_entries=static_cast<uint32_t>(c->resources.texture_cache_size());out->model_cache_entries=static_cast<uint32_t>(c->resources.model_cache_size());out->asset_count=static_cast<uint32_t>(c->assets.assets().size());return RHIZO_SUCCESS;
}

RhizoResult rhizo_texture_load(RhizoContext*c,const char*path,RhizoTexture**out){
 if(!c||!path||!*path||!out)return RHIZO_ERROR_INVALID_ARGUMENT;*out=nullptr;std::string error;auto resource=c->resources.load_texture(path,error);if(!resource){c->last_error=std::move(error);return RHIZO_ERROR_INTERNAL;}
 auto handle=new(std::nothrow) RhizoTexture();if(!handle)return RHIZO_ERROR_OUT_OF_MEMORY;handle->resource=std::move(resource);*out=handle;c->last_error.clear();return RHIZO_SUCCESS;
}
RhizoResult rhizo_texture_get_info(const RhizoTexture*t,uint32_t*w,uint32_t*h,uint64_t*bytes){
 if(!t||!t->resource||!w||!h||!bytes)return RHIZO_ERROR_INVALID_ARGUMENT;*w=t->resource->width;*h=t->resource->height;*bytes=static_cast<uint64_t>(t->resource->rgba.size());return RHIZO_SUCCESS;
}
RhizoResult rhizo_texture_copy_pixels(const RhizoTexture*t,void*dst,uint64_t capacity){
 if(!t||!t->resource||!dst||capacity<t->resource->rgba.size())return RHIZO_ERROR_INVALID_ARGUMENT;std::memcpy(dst,t->resource->rgba.data(),t->resource->rgba.size());return RHIZO_SUCCESS;
}
void rhizo_texture_release(RhizoTexture*t){delete t;}

RhizoResult rhizo_scene_component_set(RhizoContext*c,uint32_t entity,uint32_t type,const void*data,uint32_t size){
 if(!c||!c->scene.get(entity)||type==0||size>(1u<<20)||(size&&data==nullptr))return RHIZO_ERROR_INVALID_ARGUMENT;
 try{auto*set=c->scene.components.get<rhizo::CustomComponentSet>(entity);if(!set)set=&c->scene.components.emplace<rhizo::CustomComponentSet>(entity);auto&payload=set->values[type];payload.resize(size);if(size)std::memcpy(payload.data(),data,size);return RHIZO_SUCCESS;}catch(const std::bad_alloc&){return RHIZO_ERROR_OUT_OF_MEMORY;}
}
RhizoResult rhizo_scene_component_get(const RhizoContext*c,uint32_t entity,uint32_t type,void*data,uint32_t capacity,uint32_t*out_size){
 if(!c||!out_size||type==0)return RHIZO_ERROR_INVALID_ARGUMENT;const auto*set=c->scene.components.get<rhizo::CustomComponentSet>(entity);if(!set)return RHIZO_ERROR_INVALID_ARGUMENT;auto i=set->values.find(type);if(i==set->values.end())return RHIZO_ERROR_INVALID_ARGUMENT;
 *out_size=static_cast<uint32_t>(i->second.size());if(!data&&capacity==0)return RHIZO_SUCCESS;if(capacity<i->second.size()||(!data&&!i->second.empty()))return RHIZO_ERROR_INVALID_ARGUMENT;if(!i->second.empty())std::memcpy(data,i->second.data(),i->second.size());return RHIZO_SUCCESS;
}
RhizoResult rhizo_scene_component_remove(RhizoContext*c,uint32_t entity,uint32_t type){
 if(!c||type==0)return RHIZO_ERROR_INVALID_ARGUMENT;auto*set=c->scene.components.get<rhizo::CustomComponentSet>(entity);if(!set||set->values.erase(type)==0)return RHIZO_ERROR_INVALID_ARGUMENT;if(set->values.empty())c->scene.components.remove<rhizo::CustomComponentSet>(entity);return RHIZO_SUCCESS;
}
uint8_t rhizo_scene_component_has(const RhizoContext*c,uint32_t entity,uint32_t type){
 if(!c||type==0)return 0;const auto*set=c->scene.components.get<rhizo::CustomComponentSet>(entity);return set&&set->values.contains(type)?1u:0u;
}
}  // extern "C"
