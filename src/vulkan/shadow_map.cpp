#include "rhizo/shadow_map.hpp"
#include "vulkan_context.hpp"
#include <array>
#include <string>

namespace rhizo {
ShadowMap::~ShadowMap() { destroy(); }

bool ShadowMap::create(VulkanContext& context, uint32_t width, uint32_t height, std::string& error) {
  destroy();
  if (width == 0 || height == 0 || width > 8192 || height > 8192) {
    error = "Shadow-map dimensions must be between 1 and 8192";
    return false;
  }
  const VkPhysicalDevice physical = context.physical_device();
  const VkDevice device = context.device();
  if (physical == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
    error = "Vulkan device is not initialized";
    return false;
  }
  constexpr std::array<VkFormat, 3> candidates = {
      VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
  for (VkFormat candidate : candidates) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physical, candidate, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0 &&
        (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0) {
      format_ = candidate;
      break;
    }
  }
  if (format_ == VK_FORMAT_UNDEFINED) {
    error = "Device has no depth format supporting both attachment and sampling";
    return false;
  }
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = format_;
  image_info.extent = {width, height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkResult result = vkCreateImage(device, &image_info, nullptr, &image_);
  if (result != VK_SUCCESS) {
    error = "vkCreateImage for shadow map failed: " + std::to_string(static_cast<int>(result));
    destroy();
    return false;
  }
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(device, image_, &requirements);
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
  uint32_t memory_type = UINT32_MAX;
  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
        (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
      memory_type = i;
      break;
    }
  }
  if (memory_type == UINT32_MAX) {
    error = "No device-local memory type is available for shadow map";
    destroy();
    return false;
  }
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = memory_type;
  result = vkAllocateMemory(device, &allocation, nullptr, &memory_);
  if (result != VK_SUCCESS) {
    error = "vkAllocateMemory for shadow map failed: " + std::to_string(static_cast<int>(result));
    destroy();
    return false;
  }
  result = vkBindImageMemory(device, image_, memory_, 0);
  if (result != VK_SUCCESS) {
    error = "vkBindImageMemory for shadow map failed: " + std::to_string(static_cast<int>(result));
    destroy();
    return false;
  }
  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image_;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format_;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;
  result = vkCreateImageView(device, &view_info, nullptr, &view_);
  if (result != VK_SUCCESS) {
    error = "vkCreateImageView for shadow map failed: " + std::to_string(static_cast<int>(result));
    destroy();
    return false;
  }
  context_ = &context;
  width_ = width;
  height_ = height;
  return true;
}

void ShadowMap::destroy() noexcept {
  if (context_ != nullptr && context_->device() != VK_NULL_HANDLE) {
    const VkDevice device = context_->device();
    if (view_ != VK_NULL_HANDLE) vkDestroyImageView(device, view_, nullptr);
    if (image_ != VK_NULL_HANDLE) vkDestroyImage(device, image_, nullptr);
    if (memory_ != VK_NULL_HANDLE) vkFreeMemory(device, memory_, nullptr);
  }
  context_ = nullptr;
  image_ = VK_NULL_HANDLE;
  view_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  width_ = height_ = 0;
}
}
