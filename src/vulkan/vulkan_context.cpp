#include "vulkan_context.hpp"
#include <algorithm>
#include <limits>
#include <vector>
namespace rhizo {
VulkanContext::~VulkanContext() { reset(); }
void VulkanContext::set_error(const char* op, VkResult result) {
  last_error_=op; last_error_+=" failed (VkResult "; last_error_+=std::to_string(static_cast<int32_t>(result)); last_error_+=")";
}
void VulkanContext::reset() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    if (command_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_,command_pool_,nullptr);
    vkDestroyDevice(device_,nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_,nullptr);
  instance_=VK_NULL_HANDLE; physical_device_=VK_NULL_HANDLE; device_=VK_NULL_HANDLE;
  graphics_queue_=VK_NULL_HANDLE; command_pool_=VK_NULL_HANDLE; graphics_queue_family_=UINT32_MAX;
  device_info_={};
}
bool VulkanContext::select_physical_device(uint32_t requested_api) {
  uint32_t count=0;
  VkResult result=vkEnumeratePhysicalDevices(instance_,&count,nullptr);
  if (result!=VK_SUCCESS || count==0) {
    if (result!=VK_SUCCESS) set_error("vkEnumeratePhysicalDevices",result);
    else last_error_="No Vulkan physical devices found";
    return false;
  }
  std::vector<VkPhysicalDevice> devices(count);
  result=vkEnumeratePhysicalDevices(instance_,&count,devices.data());
  if (result!=VK_SUCCESS) { set_error("vkEnumeratePhysicalDevices(list)",result); return false; }
  int best=std::numeric_limits<int>::min();
  for (VkPhysicalDevice candidate:devices) {
    VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(candidate,&p);
    if (p.apiVersion<requested_api) continue;
    uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr);
    std::vector<VkQueueFamilyProperties> q(n); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,q.data());
    for (uint32_t i=0;i<n;++i) {
      if (!(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT) || q[i].queueCount==0) continue;
      int score=0;
      if (p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score=400;
      else if (p.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score=300;
      else if (p.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score=100;
      if (score>best) {
        best=score; physical_device_=candidate; graphics_queue_family_=i;
        device_info_.vendor_id=p.vendorID; device_info_.device_id=p.deviceID;
        device_info_.api_version=p.apiVersion; device_info_.driver_version=p.driverVersion;
        device_info_.device_type=static_cast<uint32_t>(p.deviceType);
        device_info_.device_name=p.deviceName; device_info_.driver_name="Vulkan driver";
      }
      break;
    }
  }
  if (physical_device_==VK_NULL_HANDLE) { last_error_="No compatible Vulkan graphics device for requested API"; return false; }
  return true;
}
VulkanStatus VulkanContext::initialize(const char* app_name,uint32_t requested_api) {
  reset(); last_error_.clear();
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName=app_name; app.applicationVersion=VK_MAKE_API_VERSION(0,0,1,0);
  app.pEngineName="RHIZO"; app.engineVersion=VK_MAKE_API_VERSION(0,0,1,0);
  app.apiVersion=std::min(requested_api,static_cast<uint32_t>(VK_API_VERSION_1_1));
  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app;
  VkResult result=vkCreateInstance(&ci,nullptr,&instance_);
  if (result!=VK_SUCCESS) { set_error("vkCreateInstance",result); reset(); return VulkanStatus::Failure; }
  if (!select_physical_device(app.apiVersion)) {
    const bool unsupported=last_error_.find("No compatible")!=std::string::npos || last_error_.find("No Vulkan")!=std::string::npos;
    const std::string error=last_error_; reset(); last_error_=error;
    return unsupported?VulkanStatus::UnsupportedDevice:VulkanStatus::Failure;
  }
  const float priority=1.0F;
  VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qi.queueFamilyIndex=graphics_queue_family_; qi.queueCount=1; qi.pQueuePriorities=&priority;
  VkPhysicalDeviceFeatures supported{}; vkGetPhysicalDeviceFeatures(physical_device_,&supported);
  VkPhysicalDeviceFeatures enabled{}; enabled.samplerAnisotropy=supported.samplerAnisotropy;
  VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi; di.pEnabledFeatures=&enabled;
  result=vkCreateDevice(physical_device_,&di,nullptr,&device_);
  if (result!=VK_SUCCESS) { set_error("vkCreateDevice",result); reset(); return VulkanStatus::Failure; }
  vkGetDeviceQueue(device_,graphics_queue_family_,0,&graphics_queue_);
  VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pi.queueFamilyIndex=graphics_queue_family_;
  result=vkCreateCommandPool(device_,&pi,nullptr,&command_pool_);
  if (result!=VK_SUCCESS) { set_error("vkCreateCommandPool",result); reset(); return VulkanStatus::Failure; }
  return VulkanStatus::Success;
}
}
