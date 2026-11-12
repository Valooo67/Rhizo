#pragma once
#include "vulkan_context.hpp"
#include <android/native_window.h>
#include <vector>
#include <string>
namespace rhizo {
class Viewport final {
 public:
  ~Viewport(); bool attach(VulkanContext&,ANativeWindow*,std::string&); bool render(float,float,float,float,std::string&); void detach()noexcept;
  uint32_t width()const{return extent_.width;} uint32_t height()const{return extent_.height;}
 private:
  bool create_swapchain(std::string&); void destroy_swapchain()noexcept;
  VulkanContext* context_=nullptr; ANativeWindow* window_=nullptr; VkSurfaceKHR surface_=VK_NULL_HANDLE; VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
  VkFormat format_=VK_FORMAT_UNDEFINED; VkExtent2D extent_{}; std::vector<VkImage> images_; std::vector<VkImageView> views_;
  VkRenderPass render_pass_=VK_NULL_HANDLE; std::vector<VkFramebuffer> framebuffers_; VkCommandBuffer command_=VK_NULL_HANDLE;
  VkSemaphore acquired_=VK_NULL_HANDLE,finished_=VK_NULL_HANDLE; VkFence fence_=VK_NULL_HANDLE;
};
}