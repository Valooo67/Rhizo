#pragma once
#include <vulkan/vulkan.h>
#include <string>
namespace rhizo {
enum class VulkanStatus { Success, Failure, UnsupportedDevice };
struct VulkanDeviceInfo {
  uint32_t vendor_id = 0, device_id = 0, api_version = 0, driver_version = 0, device_type = 0;
  std::string device_name, driver_name;
};
class VulkanContext final {
 public:
  VulkanContext() = default;
  ~VulkanContext();
  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanStatus initialize(const char* application_name, uint32_t requested_api);
  const VulkanDeviceInfo& device_info() const noexcept { return device_info_; }
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
};
}
