#pragma once
#include "vulkan_context.hpp"
#include "rhizo/scene.hpp"
#include "rhizo/rhizo.h"
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
  bool render_scene(const Scene&,const Mat4&,uint32_t selected_entity,uint32_t gizmo_mode,std::string&);
  void detach() noexcept;
  void set_render_settings(const RhizoRenderSettings& settings) noexcept { settings_ = settings; }
  uint32_t width()const{return extent_.width;}
  uint32_t height()const{return extent_.height;}
 private:
  bool create_swapchain(std::string&);
  bool create_pipeline(std::string&);
  void destroy_pipeline() noexcept;
  void destroy_swapchain() noexcept;
  struct MeshGpuCache {
    std::unique_ptr<GpuBuffer> vertices,indices;
    const Vertex* source_vertices=nullptr;
    const uint32_t* source_indices=nullptr;
    size_t vertex_count=0,index_count=0;
    Vec4 base_color{};
    float metallic=-1.0f,roughness=-1.0f;
  };
  VulkanContext* context_=nullptr;
  ANativeWindow* window_=nullptr;
  VkSurfaceKHR surface_=VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
  VkFormat format_=VK_FORMAT_UNDEFINED,depth_format_=VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> views_;
  std::vector<VkFramebuffer> framebuffers_;
  VkImage depth_image_=VK_NULL_HANDLE;
  VkDeviceMemory depth_memory_=VK_NULL_HANDLE;
  VkImageView depth_view_=VK_NULL_HANDLE;
  VkRenderPass render_pass_=VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_=VK_NULL_HANDLE;
  VkPipeline triangle_pipeline_=VK_NULL_HANDLE,triangle_no_cull_pipeline_=VK_NULL_HANDLE,line_pipeline_=VK_NULL_HANDLE;
  VkCommandBuffer command_=VK_NULL_HANDLE;
  VkSemaphore acquired_=VK_NULL_HANDLE,finished_=VK_NULL_HANDLE;
  VkFence fence_=VK_NULL_HANDLE;
  std::vector<MeshGpuCache> mesh_cache_;
  std::vector<std::unique_ptr<GpuBuffer>> transient_buffers_;
  RhizoRenderSettings settings_{sizeof(RhizoRenderSettings),RHIZO_SHADOWS_MAPS,2048,0.0015f,0.01f,1.0f,1.0f,1.0f,1,1,1,1,{0,0,0,0}};
  std::array<float,4> clear_color_{{0.055f,0.065f,0.085f,1.0f}};
};
}