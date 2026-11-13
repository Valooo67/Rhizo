#pragma once
#include "vulkan_context.hpp"
#include "rhizo/scene.hpp"
#include "rhizo/rhizo.h"
#include "rhizo/shadow_map.hpp"
#include "frame_texture.hpp"
#include <android/native_window.h>
#include <vector>
#include <memory>
#include <string>
#include <array>
namespace rhizo {
class GpuBuffer;
class Viewport final {
 public:
  ~Viewport();
  bool attach(VulkanContext&,ANativeWindow*,std::string&);
  bool render(float,float,float,float,std::string&);
  bool render_scene(const Scene&,const Mat4&,uint32_t selected_entity,uint32_t gizmo_mode,std::string&,FrameTexture* opengl_frame=nullptr,uint64_t opengl_frame_index=0,const Vec3* camera_override=nullptr);
  void detach() noexcept;
  void set_render_settings(const RhizoRenderSettings& settings) noexcept { settings_ = settings; }
  uint32_t width()const{return extent_.width;}
  uint32_t height()const{return extent_.height;}
 private:
  bool create_swapchain(std::string&);
  bool create_pipeline(std::string&);
  bool ensure_shadow_map(std::string&);
  bool create_scene_descriptors(std::string&);
  bool create_environment_probe(std::string&);
  void destroy_environment_probe() noexcept;
  void update_scene_descriptors();
  bool ensure_material_descriptors(const Scene&,std::string&);
  void destroy_pipeline() noexcept;
  void destroy_swapchain() noexcept;
  struct MeshGpuCache {
    std::unique_ptr<GpuBuffer> vertices,indices;
    std::array<std::unique_ptr<GpuBuffer>,2> lod_indices;
    std::array<std::unique_ptr<GpuBuffer>,3> instance_buffers;
    std::array<uint32_t,2> lod_index_counts{};
    const Vertex* source_vertices=nullptr;
    const uint32_t* source_indices=nullptr;
    size_t vertex_count=0,index_count=0;
    Vec4 base_color{};
    float metallic=-1.0f,roughness=-1.0f,normal_scale=-1.0f,occlusion=-1.0f;
    Vec3 emissive_color{};
    Vec3 bounds_center{};
    float bounds_radius=0.0f;
  };
  struct MaterialGpuCache { std::array<std::unique_ptr<FrameTexture>,4> textures; std::array<std::string,4> paths; VkDescriptorSet descriptor=VK_NULL_HANDLE; bool initialized=false; };
  struct DrawItem { const Mesh* mesh=nullptr; Mat4 model=Mat4::identity(); uint32_t meshIndex=0; EntityId entity=0; VkDescriptorSet skin_descriptor=VK_NULL_HANDLE,morph_descriptor=VK_NULL_HANDLE; bool skinned=false,morphed=false; };
  struct SkinGpuCache { EntityId entity=0; uint32_t mesh_index=0,bone_count=0; std::unique_ptr<GpuBuffer> matrices; VkDescriptorSet descriptor=VK_NULL_HANDLE; };
  std::vector<SkinGpuCache> skin_cache_;
  struct MorphGpuCache { EntityId entity=0; uint32_t mesh_index=0; std::unique_ptr<GpuBuffer> data; VkDescriptorSet descriptor=VK_NULL_HANDLE; };
  std::vector<MorphGpuCache> morph_cache_;
  std::vector<DrawItem> draw_items_;
  std::vector<std::array<std::vector<Mat4>,3>> instance_batches_;
  VkDescriptorSetLayout material_set_layout_=VK_NULL_HANDLE,skin_set_layout_=VK_NULL_HANDLE,morph_set_layout_=VK_NULL_HANDLE;
  VkDescriptorPool skin_descriptor_pool_=VK_NULL_HANDLE,morph_descriptor_pool_=VK_NULL_HANDLE;
  VkDescriptorSet identity_skin_descriptor_set_=VK_NULL_HANDLE,identity_morph_descriptor_set_=VK_NULL_HANDLE;
  std::unique_ptr<GpuBuffer> identity_skin_buffer_,identity_morph_buffer_;
  VkDescriptorSet frame_descriptor_set_=VK_NULL_HANDLE;
  VkDescriptorPool material_descriptor_pool_=VK_NULL_HANDLE;
  std::vector<MaterialGpuCache> material_cache_;
  std::array<std::unique_ptr<FrameTexture>,4> default_textures_;
  VulkanContext* context_=nullptr;
  ANativeWindow* window_=nullptr;
  VkSurfaceKHR surface_=VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
  VkFormat format_=VK_FORMAT_UNDEFINED,hdr_format_=VK_FORMAT_UNDEFINED,depth_format_=VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> views_;
  std::vector<VkFramebuffer> framebuffers_,present_framebuffers_;
  std::vector<VkImage> hdr_images_;
  std::vector<VkDeviceMemory> hdr_memories_;
  std::vector<VkImageView> hdr_views_;
  VkImage depth_image_=VK_NULL_HANDLE;
  VkDeviceMemory depth_memory_=VK_NULL_HANDLE;
  VkImageView depth_view_=VK_NULL_HANDLE;
  VkRenderPass render_pass_=VK_NULL_HANDLE,present_render_pass_=VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_=VK_NULL_HANDLE,composite_pipeline_layout_=VK_NULL_HANDLE;
  VkDescriptorSetLayout composite_set_layout_=VK_NULL_HANDLE;
  VkDescriptorPool composite_descriptor_pool_=VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> composite_descriptor_sets_;
  VkSampler hdr_sampler_=VK_NULL_HANDLE;
  VkPipeline composite_pipeline_=VK_NULL_HANDLE,triangle_pipeline_=VK_NULL_HANDLE,triangle_no_cull_pipeline_=VK_NULL_HANDLE,triangle_instanced_pipeline_=VK_NULL_HANDLE,triangle_instanced_no_cull_pipeline_=VK_NULL_HANDLE,line_pipeline_=VK_NULL_HANDLE,shadow_pipeline_=VK_NULL_HANDLE,skybox_pipeline_=VK_NULL_HANDLE;
  VkPipelineLayout shadow_pipeline_layout_=VK_NULL_HANDLE;
  VkDescriptorSetLayout scene_set_layout_=VK_NULL_HANDLE;
  VkDescriptorPool scene_descriptor_pool_=VK_NULL_HANDLE;
  VkDescriptorSet scene_descriptor_set_=VK_NULL_HANDLE,probe_scene_descriptor_set_=VK_NULL_HANDLE;
  std::unique_ptr<GpuBuffer> scene_uniform_,probe_scene_uniform_;
  std::array<ShadowMap,10> shadow_maps_;
  uint64_t shadow_target_signature_=0;
  bool shadow_cache_valid_=false;
  VkImage environment_image_=VK_NULL_HANDLE,environment_depth_image_=VK_NULL_HANDLE,environment_dummy_image_=VK_NULL_HANDLE;
  VkDeviceMemory environment_memory_=VK_NULL_HANDLE,environment_depth_memory_=VK_NULL_HANDLE,environment_dummy_memory_=VK_NULL_HANDLE;
  VkImageView environment_view_=VK_NULL_HANDLE,environment_depth_view_=VK_NULL_HANDLE,environment_dummy_view_=VK_NULL_HANDLE;
  std::array<VkImageView,6> environment_face_views_{};
  std::array<VkFramebuffer,6> environment_framebuffers_{};
  VkSampler environment_sampler_=VK_NULL_HANDLE;
  VkRenderPass environment_render_pass_=VK_NULL_HANDLE;
  std::vector<DrawItem> all_draw_items_;
  uint64_t environment_target_signature_=0;
  uint32_t environment_capture_face_=0;
  bool environment_capture_pending_=false;
  bool environment_valid_=false;
  uint32_t environment_resolution_=128;
  VkCommandBuffer command_=VK_NULL_HANDLE;
  VkSemaphore acquired_=VK_NULL_HANDLE,finished_=VK_NULL_HANDLE;
  VkFence fence_=VK_NULL_HANDLE;
  std::vector<MeshGpuCache> mesh_cache_;
  std::unique_ptr<GpuBuffer> grid_buffer_,gizmo_buffer_,frame_plane_vertices_,frame_plane_indices_;
  RhizoRenderSettings settings_{sizeof(RhizoRenderSettings),RHIZO_SHADOWS_MAPS,2048,0.0015f,0.01f,1.0f,1.0f,1.0f,1,1,1,1,{0,0,0,0}};
  std::array<float,4> clear_color_{{0.055f,0.065f,0.085f,1.0f}};
};
}