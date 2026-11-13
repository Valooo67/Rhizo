#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace rhizo {
class VulkanContext;

class ShadowMap final {
 public:
  ShadowMap() = default;
  ~ShadowMap();
  ShadowMap(const ShadowMap&) = delete;
  ShadowMap& operator=(const ShadowMap&) = delete;
  bool create(VulkanContext& context, uint32_t width, uint32_t height, std::string& error);
  VkRenderPass render_pass() const noexcept { return render_pass_; }
  VkFramebuffer framebuffer() const noexcept { return framebuffer_; }
  VkSampler sampler() const noexcept { return sampler_; }
  void destroy() noexcept;
  VkImage image() const noexcept { return image_; }
  VkImageView view() const noexcept { return view_; }
  VkFormat format() const noexcept { return format_; }
  uint32_t width() const noexcept { return width_; }
  uint32_t height() const noexcept { return height_; }
 private:
  VulkanContext* context_ = nullptr;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  uint32_t width_ = 0, height_ = 0;
};
}
