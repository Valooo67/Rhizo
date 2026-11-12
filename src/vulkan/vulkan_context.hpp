#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <string>

namespace rhizo {

enum class VulkanStatus { Success, Failure, UnsupportedDevice };

struct VulkanDeviceInfo {
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
  uint32_t api_version = 0;
  uint32_t driver_version = 0;
  uint32_t device_type = 0;
  std::string device_name;
  std::string driver_name;
};

class VulkanContext final {
 public:
  VulkanContext() = default;
  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;

  VulkanStatus initialize(const char* application_name, uint32_t requested_api);
  bool copy_buffer(VkBuffer source, VkBuffer destination, VkDeviceSize size,
                   std::string& error);

  const VulkanDeviceInfo& device_info() const noexcept { return device_info_; }
  VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
  VkDevice device() const noexcept { return device_; }
  const std::string& last_error() const noexcept { return last_error_; }

 private:
  void reset() noexcept;
  bool select_physical_device(uint32_t requested_api);
  void set_error(const char* operation, VkResult result);

  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  uint32_t graphics_queue_family_ = UINT32_MAX;
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  VulkanDeviceInfo device_info_;
  std::string last_error_;
  std::mutex queue_mutex_;
};

}  // namespace rhizo
