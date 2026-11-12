#include "gpu_buffer.hpp"
#include "vulkan_context.hpp"

#include <algorithm>
#include <cstring>

namespace rhizo {

GpuBuffer::~GpuBuffer() { destroy(); }

bool GpuBuffer::find_memory_type(uint32_t bits, VkMemoryPropertyFlags required,
                                 VkMemoryPropertyFlags preferred, uint32_t& index,
                                 VkMemoryPropertyFlags& selected) const {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_, &properties);

  int best_score = -1;
  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((bits & (1u << i)) == 0) {
      continue;
    }

    const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
    if ((flags & required) != required) {
      continue;
    }

    const int score = (flags & preferred) == preferred ? 2 : 1;
    if (score > best_score) {
      best_score = score;
      index = i;
      selected = flags;
    }
  }
  return best_score >= 0;
}

bool GpuBuffer::create(VulkanContext& context, VkDeviceSize requested_size,
                       VkBufferUsageFlags usage, bool host_visible,
                       std::string& error) {
  destroy();

  physical_ = context.physical_device();
  device_ = context.device();
  context_ = &context;
  host_visible_ = host_visible;

  if (physical_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE ||
      requested_size == 0 || usage == 0) {
    error = "Invalid Vulkan buffer creation parameters";
    destroy();
    return false;
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_, &properties);
  non_coherent_atom_size_ =
      std::max<VkDeviceSize>(1, properties.limits.nonCoherentAtomSize);

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = requested_size;
  buffer_info.usage = usage | (host_visible ? 0u : VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult result = vkCreateBuffer(device_, &buffer_info, nullptr, &buffer_);
  if (result != VK_SUCCESS) {
    error = "vkCreateBuffer failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    destroy();
    return false;
  }

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, buffer_, &requirements);

  const VkMemoryPropertyFlags required = host_visible
      ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const VkMemoryPropertyFlags preferred = host_visible
      ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  uint32_t memory_index = 0;
  VkMemoryPropertyFlags selected = 0;
  if (!find_memory_type(requirements.memoryTypeBits, required, preferred,
                        memory_index, selected)) {
    error = host_visible
        ? "No host-visible Vulkan memory type is available for buffer uploads"
        : "No device-local Vulkan memory type is available for GPU buffer";
    destroy();
    return false;
  }

  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = memory_index;

  result = vkAllocateMemory(device_, &allocation, nullptr, &memory_);
  if (result != VK_SUCCESS) {
    error = "vkAllocateMemory for buffer failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    destroy();
    return false;
  }

  result = vkBindBufferMemory(device_, buffer_, memory_, 0);
  if (result != VK_SUCCESS) {
    error = "vkBindBufferMemory failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    destroy();
    return false;
  }

  size_ = requested_size;
  allocation_size_ = requirements.size;
  coherent_ = (selected & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
  return true;
}

bool GpuBuffer::write(VkDeviceSize offset, const void* data, VkDeviceSize length,
                      std::string& error) {
  if (device_ == VK_NULL_HANDLE || memory_ == VK_NULL_HANDLE || !data ||
      length == 0 || offset > size_ || length > size_ - offset) {
    error = "Invalid Vulkan buffer write range or data";
    return false;
  }

  if (!host_visible_) {
    GpuBuffer staging;
    if (!staging.create(*context_, length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        true, error)) {
      return false;
    }
    if (!staging.write(0, data, length, error)) {
      return false;
    }
    return context_->copy_buffer(staging.handle(), buffer_, length, error);
  }

  void* mapped = nullptr;
  VkResult result = vkMapMemory(device_, memory_, 0, VK_WHOLE_SIZE, 0, &mapped);
  if (result != VK_SUCCESS) {
    error = "vkMapMemory failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    return false;
  }

  std::memcpy(static_cast<unsigned char*>(mapped) + offset, data,
              static_cast<size_t>(length));

  if (!coherent_) {
    const VkDeviceSize begin =
        (offset / non_coherent_atom_size_) * non_coherent_atom_size_;
    const VkDeviceSize write_end = offset + length;
    const VkDeviceSize aligned_end =
        ((write_end + non_coherent_atom_size_ - 1) / non_coherent_atom_size_) *
        non_coherent_atom_size_;
    const VkDeviceSize end = std::min(allocation_size_, aligned_end);

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = memory_;
    range.offset = begin;
    range.size = end == allocation_size_ ? VK_WHOLE_SIZE : end - begin;
    result = vkFlushMappedMemoryRanges(device_, 1, &range);
  }

  vkUnmapMemory(device_, memory_);
  if (result != VK_SUCCESS) {
    error = "vkFlushMappedMemoryRanges failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    return false;
  }
  return true;
}

void GpuBuffer::destroy() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    if (buffer_ != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_, buffer_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
      vkFreeMemory(device_, memory_, nullptr);
    }
  }

  context_ = nullptr;
  physical_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  buffer_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  size_ = 0;
  allocation_size_ = 0;
  coherent_ = false;
  host_visible_ = false;
  non_coherent_atom_size_ = 1;
}

}  // namespace rhizo
