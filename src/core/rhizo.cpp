#include "rhizo/rhizo.h"

#include "vulkan/gpu_buffer.hpp"
#include "vulkan/vulkan_context.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

struct RhizoBuffer;

struct RhizoContext {
  rhizo::VulkanContext vulkan;
  std::string last_error;
  std::vector<RhizoBuffer*> buffers;
};

struct RhizoBuffer {
  RhizoContext* owner = nullptr;
  rhizo::GpuBuffer gpu;
};

namespace {

constexpr uint32_t kUsageMask =
    RHIZO_BUFFER_VERTEX | RHIZO_BUFFER_INDEX | RHIZO_BUFFER_UNIFORM |
    RHIZO_BUFFER_STORAGE | RHIZO_BUFFER_INDIRECT;

constexpr uint32_t kMemoryMask =
    RHIZO_BUFFER_CPU_VISIBLE | RHIZO_BUFFER_GPU_ONLY;

VkBufferUsageFlags to_vk_usage(uint32_t usage) {
  VkBufferUsageFlags flags = 0;
  if ((usage & RHIZO_BUFFER_VERTEX) != 0) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_INDEX) != 0) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_UNIFORM) != 0) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_STORAGE) != 0) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  if ((usage & RHIZO_BUFFER_INDIRECT) != 0) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  return flags;
}

}  // namespace

extern "C" {

uint32_t rhizo_get_abi_version(void) {
  return RHIZO_ABI_VERSION;
}

RhizoResult rhizo_create(const RhizoCreateInfo* info, RhizoContext** out_context) {
  if (out_context == nullptr) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }
  *out_context = nullptr;

  if (info == nullptr || info->struct_size < sizeof(RhizoCreateInfo) ||
      info->abi_version != RHIZO_ABI_VERSION) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  auto context = std::unique_ptr<RhizoContext>(new (std::nothrow) RhizoContext());
  if (!context) {
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  const char* application =
      info->application_name != nullptr && info->application_name[0] != '\0'
          ? info->application_name
          : "RHIZO";
  const uint32_t api = info->requested_vulkan_api != 0
      ? info->requested_vulkan_api
      : VK_API_VERSION_1_1;

  const rhizo::VulkanStatus status =
      context->vulkan.initialize(application, api);
  if (status != rhizo::VulkanStatus::Success) {
    context->last_error = context->vulkan.last_error();
    return status == rhizo::VulkanStatus::UnsupportedDevice
        ? RHIZO_ERROR_UNSUPPORTED_DEVICE
        : RHIZO_ERROR_VULKAN;
  }

  *out_context = context.release();
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_get_device_info(const RhizoContext* context,
                                  RhizoDeviceInfo* out_info) {
  if (context == nullptr || out_info == nullptr ||
      out_info->struct_size < sizeof(RhizoDeviceInfo)) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  const rhizo::VulkanDeviceInfo& source = context->vulkan.device_info();
  out_info->vendor_id = source.vendor_id;
  out_info->device_id = source.device_id;
  out_info->api_version = source.api_version;
  out_info->driver_version = source.driver_version;
  out_info->device_type = source.device_type;

  std::memset(out_info->device_name, 0, sizeof(out_info->device_name));
  std::memset(out_info->driver_name, 0, sizeof(out_info->driver_name));
  std::strncpy(out_info->device_name, source.device_name.c_str(),
               sizeof(out_info->device_name) - 1);
  std::strncpy(out_info->driver_name, source.driver_name.c_str(),
               sizeof(out_info->driver_name) - 1);
  return RHIZO_SUCCESS;
}

const char* rhizo_get_last_error(const RhizoContext* context) {
  return context == nullptr ? "Invalid RHIZO context" : context->last_error.c_str();
}

RhizoResult rhizo_buffer_create(RhizoContext* context,
                                const RhizoBufferCreateInfo* info,
                                RhizoBuffer** out_buffer) {
  if (out_buffer == nullptr) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }
  *out_buffer = nullptr;

  if (context == nullptr || info == nullptr ||
      info->struct_size < sizeof(RhizoBufferCreateInfo) ||
      info->size == 0 || info->size > static_cast<uint64_t>(SIZE_MAX) ||
      (info->usage & kUsageMask) == 0 ||
      (info->usage & ~(kUsageMask | kMemoryMask)) != 0 ||
      (info->usage & kMemoryMask) == kMemoryMask) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  const bool host_visible =
      (info->usage & RHIZO_BUFFER_GPU_ONLY) == 0;

  auto buffer = std::unique_ptr<RhizoBuffer>(new (std::nothrow) RhizoBuffer());
  if (!buffer) {
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  std::string error;
  if (!buffer->gpu.create(context->vulkan, static_cast<VkDeviceSize>(info->size),
                          to_vk_usage(info->usage), host_visible, error)) {
    context->last_error = std::move(error);
    return RHIZO_ERROR_VULKAN;
  }

  buffer->owner = context;
  try {
    context->buffers.push_back(buffer.get());
  } catch (...) {
    context->last_error = "Unable to register RHIZO buffer";
    return RHIZO_ERROR_OUT_OF_MEMORY;
  }

  *out_buffer = buffer.release();
  return RHIZO_SUCCESS;
}

RhizoResult rhizo_buffer_write(RhizoBuffer* buffer, uint64_t offset,
                               const void* data, uint64_t size) {
  if (buffer == nullptr || buffer->owner == nullptr ||
      offset > static_cast<uint64_t>(SIZE_MAX) ||
      size > static_cast<uint64_t>(SIZE_MAX)) {
    return RHIZO_ERROR_INVALID_ARGUMENT;
  }

  std::string error;
  if (!buffer->gpu.write(static_cast<VkDeviceSize>(offset), data,
                         static_cast<VkDeviceSize>(size), error)) {
    buffer->owner->last_error = std::move(error);
    return RHIZO_ERROR_VULKAN;
  }
  return RHIZO_SUCCESS;
}

uint64_t rhizo_buffer_get_size(const RhizoBuffer* buffer) {
  return buffer == nullptr ? 0 : static_cast<uint64_t>(buffer->gpu.size());
}

void rhizo_buffer_destroy(RhizoBuffer* buffer) {
  if (buffer == nullptr) {
    return;
  }

  if (buffer->owner != nullptr) {
    auto& buffers = buffer->owner->buffers;
    buffers.erase(std::remove(buffers.begin(), buffers.end(), buffer), buffers.end());
    buffer->owner = nullptr;
  }
  delete buffer;
}

void rhizo_destroy(RhizoContext* context) {
  if (context == nullptr) {
    return;
  }

  for (RhizoBuffer* buffer : context->buffers) {
    if (buffer != nullptr) {
      buffer->gpu.destroy();
      buffer->owner = nullptr;
    }
  }
  context->buffers.clear();
  delete context;
}

}  // extern "C"
