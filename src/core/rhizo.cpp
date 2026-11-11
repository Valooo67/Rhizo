#include "rhizo/rhizo.h"
#include "vulkan/vulkan_context.hpp"
#include <cstring>
#include <memory>
#include <new>
#include <string>
struct RhizoContext { rhizo::VulkanContext vulkan; std::string last_error; };
extern "C" {
uint32_t rhizo_get_abi_version(void) { return RHIZO_ABI_VERSION; }
RhizoResult rhizo_create(const RhizoCreateInfo* info, RhizoContext** out_context) {
  if (!out_context) return RHIZO_ERROR_INVALID_ARGUMENT;
  *out_context = nullptr;
  if (!info || info->struct_size < sizeof(RhizoCreateInfo) || info->abi_version != RHIZO_ABI_VERSION)
    return RHIZO_ERROR_INVALID_ARGUMENT;
  auto context = std::unique_ptr<RhizoContext>(new (std::nothrow) RhizoContext());
  if (!context) return RHIZO_ERROR_OUT_OF_MEMORY;
  const char* app = info->application_name && info->application_name[0] ? info->application_name : "RHIZO";
  const uint32_t api = info->requested_vulkan_api ? info->requested_vulkan_api : VK_API_VERSION_1_1;
  const auto status = context->vulkan.initialize(app, api);
  if (status != rhizo::VulkanStatus::Success) {
    context->last_error = context->vulkan.last_error();
    return status == rhizo::VulkanStatus::UnsupportedDevice ? RHIZO_ERROR_UNSUPPORTED_DEVICE : RHIZO_ERROR_VULKAN;
  }
  *out_context = context.release();
  return RHIZO_SUCCESS;
}
RhizoResult rhizo_get_device_info(const RhizoContext* context, RhizoDeviceInfo* out) {
  if (!context || !out || out->struct_size < sizeof(RhizoDeviceInfo)) return RHIZO_ERROR_INVALID_ARGUMENT;
  const auto& s = context->vulkan.device_info();
  out->vendor_id=s.vendor_id; out->device_id=s.device_id; out->api_version=s.api_version;
  out->driver_version=s.driver_version; out->device_type=s.device_type;
  std::memset(out->device_name,0,sizeof(out->device_name));
  std::memset(out->driver_name,0,sizeof(out->driver_name));
  std::strncpy(out->device_name,s.device_name.c_str(),sizeof(out->device_name)-1);
  std::strncpy(out->driver_name,s.driver_name.c_str(),sizeof(out->driver_name)-1);
  return RHIZO_SUCCESS;
}
const char* rhizo_get_last_error(const RhizoContext* context) {
  return !context ? "Invalid RHIZO context" : context->last_error.c_str();
}
void rhizo_destroy(RhizoContext* context) { delete context; }
}
