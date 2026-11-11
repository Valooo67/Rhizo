#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>

namespace rhizo {
class GpuBuffer final {
 public:
  GpuBuffer() = default;
  ~GpuBuffer();
  GpuBuffer(const GpuBuffer&) = delete;
  GpuBuffer& operator=(const GpuBuffer&) = delete;
  bool create(VkPhysicalDevice physical, VkDevice device, VkDeviceSize size,
              VkBufferUsageFlags usage, std::string& error);
  bool write(VkDeviceSize offset, const void* data, VkDeviceSize size, std::string& error);
  void destroy() noexcept;
  VkBuffer handle() const noexcept { return buffer_; }
  VkDeviceSize size() const noexcept { return size_; }
 private:
  bool find_memory_type(uint32_t bits, VkMemoryPropertyFlags required,
                        VkMemoryPropertyFlags preferred, uint32_t& index,
                        VkMemoryPropertyFlags& selected) const;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkDeviceSize size_ = 0;
  VkDeviceSize allocation_size_ = 0;
  VkDeviceSize non_coherent_atom_size_ = 1;
  bool coherent_ = false;
};
}
