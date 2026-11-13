#pragma once
#include "vulkan_context.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
namespace rhizo {
class FrameTexture final {
 public:
  ~FrameTexture();
  FrameTexture(const FrameTexture&)=delete;
  FrameTexture& operator=(const FrameTexture&)=delete;
  FrameTexture()=default;
  bool upload(VulkanContext&,VkBuffer,uint32_t,uint32_t,uint32_t,bool,std::string&);
  bool upload_rgba8(VulkanContext&,const uint8_t*,uint32_t,uint32_t,std::string&);
  void destroy() noexcept;
  VkImageView view()const noexcept{return view_;}
  VkSampler sampler()const noexcept{return sampler_;}
  uint32_t width()const noexcept{return width_;}
  uint32_t height()const noexcept{return height_;}
 private:
  bool create(VulkanContext&,uint32_t,uint32_t,VkFormat,std::string&);
  VulkanContext* context_=nullptr;
  VkImage image_=VK_NULL_HANDLE;
  VkDeviceMemory memory_=VK_NULL_HANDLE;
  VkImageView view_=VK_NULL_HANDLE;
  VkSampler sampler_=VK_NULL_HANDLE;
  VkFormat format_=VK_FORMAT_UNDEFINED;
  uint32_t width_=0,height_=0;
};
}
