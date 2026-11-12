#include "vulkan_context.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace rhizo {

VulkanContext::~VulkanContext() { reset(); }

void VulkanContext::set_error(const char* operation, VkResult result) {
  last_error_ = operation;
  last_error_ += " failed (VkResult ";
  last_error_ += std::to_string(static_cast<int32_t>(result));
  last_error_ += ")";
}

void VulkanContext::reset() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    (void)vkDeviceWaitIdle(device_);
    if (command_pool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, command_pool_, nullptr);
    }
    vkDestroyDevice(device_, nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }

  instance_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  command_pool_ = VK_NULL_HANDLE;
  graphics_queue_family_ = UINT32_MAX;
  device_info_ = {};
}

bool VulkanContext::select_physical_device(uint32_t requested_api) {
  uint32_t count = 0;
  VkResult result = vkEnumeratePhysicalDevices(instance_, &count, nullptr);
  if (result != VK_SUCCESS || count == 0) {
    if (result != VK_SUCCESS) {
      set_error("vkEnumeratePhysicalDevices", result);
    } else {
      last_error_ = "No Vulkan physical devices found";
    }
    return false;
  }

  std::vector<VkPhysicalDevice> devices(count);
  result = vkEnumeratePhysicalDevices(instance_, &count, devices.data());
  if (result != VK_SUCCESS) {
    set_error("vkEnumeratePhysicalDevices(list)", result);
    return false;
  }

  int best_score = std::numeric_limits<int>::min();
  for (VkPhysicalDevice candidate : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    if (properties.apiVersion < requested_api) {
      continue;
    }

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());

    for (uint32_t index = 0; index < queue_count; ++index) {
      if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
          queues[index].queueCount == 0) {
        continue;
      }

      int score = 0;
      switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 400; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 300; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 100; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: score = 50; break;
        default: break;
      }

      if (score > best_score) {
        best_score = score;
        physical_device_ = candidate;
        graphics_queue_family_ = index;
        device_info_.vendor_id = properties.vendorID;
        device_info_.device_id = properties.deviceID;
        device_info_.api_version = properties.apiVersion;
        device_info_.driver_version = properties.driverVersion;
        device_info_.device_type = static_cast<uint32_t>(properties.deviceType);
        device_info_.device_name = properties.deviceName;
        device_info_.driver_name = "Vulkan driver";
      }
      break;
    }
  }

  if (physical_device_ == VK_NULL_HANDLE) {
    last_error_ = "No compatible Vulkan graphics device for requested API";
    return false;
  }
  return true;
}

VulkanStatus VulkanContext::initialize(const char* application_name,
                                       uint32_t requested_api) {
  reset();
  last_error_.clear();

  const uint32_t api_version =
      std::min(requested_api, static_cast<uint32_t>(VK_API_VERSION_1_1));

  VkApplicationInfo application{};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = application_name;
  application.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  application.pEngineName = "RHIZO";
  application.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  application.apiVersion = api_version;

  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &application;

  VkResult result = vkCreateInstance(&instance_info, nullptr, &instance_);
  if (result != VK_SUCCESS) {
    set_error("vkCreateInstance", result);
    reset();
    return VulkanStatus::Failure;
  }

  if (!select_physical_device(api_version)) {
    const bool unsupported =
        last_error_.find("No compatible") != std::string::npos ||
        last_error_.find("No Vulkan") != std::string::npos;
    const std::string error = last_error_;
    reset();
    last_error_ = error;
    return unsupported ? VulkanStatus::UnsupportedDevice : VulkanStatus::Failure;
  }

  const float priority = 1.0F;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = graphics_queue_family_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;

  VkPhysicalDeviceFeatures supported_features{};
  vkGetPhysicalDeviceFeatures(physical_device_, &supported_features);

  VkPhysicalDeviceFeatures enabled_features{};
  enabled_features.samplerAnisotropy = supported_features.samplerAnisotropy;

  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.pEnabledFeatures = &enabled_features;

  result = vkCreateDevice(physical_device_, &device_info, nullptr, &device_);
  if (result != VK_SUCCESS) {
    set_error("vkCreateDevice", result);
    reset();
    return VulkanStatus::Failure;
  }

  vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = graphics_queue_family_;

  result = vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_);
  if (result != VK_SUCCESS) {
    set_error("vkCreateCommandPool", result);
    reset();
    return VulkanStatus::Failure;
  }

  return VulkanStatus::Success;
}

bool VulkanContext::copy_buffer(VkBuffer source, VkBuffer destination,
                                VkDeviceSize size, std::string& error) {
  if (device_ == VK_NULL_HANDLE || graphics_queue_ == VK_NULL_HANDLE ||
      command_pool_ == VK_NULL_HANDLE || source == VK_NULL_HANDLE ||
      destination == VK_NULL_HANDLE || size == 0) {
    error = "Invalid Vulkan buffer copy parameters";
    return false;
  }

  std::lock_guard<std::mutex> lock(queue_mutex_);

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkCommandBufferAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocation.commandPool = command_pool_;
  allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocation.commandBufferCount = 1;

  VkResult result = vkAllocateCommandBuffers(device_, &allocation, &command_buffer);
  if (result != VK_SUCCESS) {
    error = "vkAllocateCommandBuffers failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    return false;
  }

  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer(command_buffer, &begin);
  if (result != VK_SUCCESS) {
    error = "vkBeginCommandBuffer failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    return false;
  }

  VkBufferCopy region{};
  region.size = size;
  vkCmdCopyBuffer(command_buffer, source, destination, 1, &region);

  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = destination;
  barrier.offset = 0;
  barrier.size = size;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                       0, nullptr, 1, &barrier, 0, nullptr);

  result = vkEndCommandBuffer(command_buffer);
  if (result != VK_SUCCESS) {
    error = "vkEndCommandBuffer failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    return false;
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  result = vkCreateFence(device_, &fence_info, nullptr, &fence);
  if (result != VK_SUCCESS) {
    error = "vkCreateFence failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    return false;
  }

  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &command_buffer;
  result = vkQueueSubmit(graphics_queue_, 1, &submit, fence);
  if (result == VK_SUCCESS) {
    result = vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
  }

  vkDestroyFence(device_, fence, nullptr);
  vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);

  if (result != VK_SUCCESS) {
    error = "Vulkan buffer copy submission failed (VkResult " +
            std::to_string(static_cast<int32_t>(result)) + ")";
    return false;
  }
  return true;
}

}  // namespace rhizo
