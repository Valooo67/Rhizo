#include "vulkan_context.hpp"
#include <algorithm>
#include <limits>
#include <vector>
namespace rhizo {
VulkanContext::~VulkanContext(){reset();}
void VulkanContext::set_error(const char*op,VkResult r){last_error_=op;last_error_+=" failed (VkResult ";last_error_+=std::to_string(static_cast<int32_t>(r));last_error_+=")";}
void VulkanContext::reset()noexcept{
 if(device_){(void)vkDeviceWaitIdle(device_);if(command_pool_)vkDestroyCommandPool(device_,command_pool_,nullptr);vkDestroyDevice(device_,nullptr);}
 if(instance_)vkDestroyInstance(instance_,nullptr);
 instance_=VK_NULL_HANDLE;physical_device_=VK_NULL_HANDLE;device_=VK_NULL_HANDLE;graphics_queue_=VK_NULL_HANDLE;command_pool_=VK_NULL_HANDLE;graphics_queue_family_=UINT32_MAX;device_info_={};
}
bool VulkanContext::select_physical_device(uint32_t api){
 uint32_t count=0;VkResult r=vkEnumeratePhysicalDevices(instance_,&count,nullptr);
 if(r!=VK_SUCCESS||!count){if(r!=VK_SUCCESS)set_error("vkEnumeratePhysicalDevices",r);else last_error_="No Vulkan physical devices found";return false;}
 std::vector<VkPhysicalDevice>d(count);r=vkEnumeratePhysicalDevices(instance_,&count,d.data());if(r!=VK_SUCCESS){set_error("vkEnumeratePhysicalDevices(list)",r);return false;}
 int best=std::numeric_limits<int>::min();
 for(auto p:d){VkPhysicalDeviceProperties prop{};vkGetPhysicalDeviceProperties(p,&prop);if(prop.apiVersion<api)continue;
  uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(p,&n,nullptr);std::vector<VkQueueFamilyProperties>q(n);vkGetPhysicalDeviceQueueFamilyProperties(p,&n,q.data());
  for(uint32_t i=0;i<n;i++){if(!(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)||!q[i].queueCount)continue;int score=prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?400:prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?300:prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU?100:prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU?50:0;
   if(score>best){best=score;physical_device_=p;graphics_queue_family_=i;device_info_.vendor_id=prop.vendorID;device_info_.device_id=prop.deviceID;device_info_.api_version=prop.apiVersion;device_info_.driver_version=prop.driverVersion;device_info_.device_type=static_cast<uint32_t>(prop.deviceType);device_info_.device_name=prop.deviceName;device_info_.driver_name="Vulkan driver";}break;}
 }
 if(!physical_device_){last_error_="No compatible Vulkan graphics device for requested API";return false;}return true;
}
VulkanStatus VulkanContext::initialize(const char*app,uint32_t requested){
 reset();last_error_.clear();const uint32_t api=std::min(requested,static_cast<uint32_t>(VK_API_VERSION_1_1));
 const char* instance_exts[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
 VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName=app;ai.applicationVersion=VK_MAKE_API_VERSION(0,0,1,0);ai.pEngineName="RHIZO";ai.engineVersion=VK_MAKE_API_VERSION(0,0,1,0);ai.apiVersion=api;
 VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ii.pApplicationInfo=&ai;ii.enabledExtensionCount=2;ii.ppEnabledExtensionNames=instance_exts;
 VkResult r=vkCreateInstance(&ii,nullptr,&instance_);if(r!=VK_SUCCESS){set_error("vkCreateInstance",r);reset();return VulkanStatus::Failure;}
 if(!select_physical_device(api)){bool u=last_error_.find("No compatible")!=std::string::npos||last_error_.find("No Vulkan")!=std::string::npos;auto e=last_error_;reset();last_error_=e;return u?VulkanStatus::UnsupportedDevice:VulkanStatus::Failure;}
 float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=graphics_queue_family_;qi.queueCount=1;qi.pQueuePriorities=&priority;
 VkPhysicalDeviceFeatures supported{};vkGetPhysicalDeviceFeatures(physical_device_,&supported);VkPhysicalDeviceFeatures enabled{};enabled.samplerAnisotropy=supported.samplerAnisotropy;
 const char* device_exts[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
 VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.pEnabledFeatures=&enabled;di.enabledExtensionCount=1;di.ppEnabledExtensionNames=device_exts;
 r=vkCreateDevice(physical_device_,&di,nullptr,&device_);if(r!=VK_SUCCESS){set_error("vkCreateDevice",r);reset();return VulkanStatus::Failure;}
 vkGetDeviceQueue(device_,graphics_queue_family_,0,&graphics_queue_);
 VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT|VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;pi.queueFamilyIndex=graphics_queue_family_;
 r=vkCreateCommandPool(device_,&pi,nullptr,&command_pool_);if(r!=VK_SUCCESS){set_error("vkCreateCommandPool",r);reset();return VulkanStatus::Failure;}return VulkanStatus::Success;
}
bool VulkanContext::copy_buffer(VkBuffer src,VkBuffer dst,VkDeviceSize size,std::string&e){
 if(!device_||!graphics_queue_||!command_pool_||!src||!dst||!size){e="Invalid Vulkan buffer copy parameters";return false;}
 std::lock_guard<std::mutex>lock(queue_mutex_);VkCommandBuffer cb=VK_NULL_HANDLE;VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};a.commandPool=command_pool_;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;
 VkResult r=vkAllocateCommandBuffers(device_,&a,&cb);if(r!=VK_SUCCESS){e="vkAllocateCommandBuffers failed";return false;}
 VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};b.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;r=vkBeginCommandBuffer(cb,&b);
 if(r==VK_SUCCESS){VkBufferCopy cp{};cp.size=size;vkCmdCopyBuffer(cb,src,dst,1,&cp);VkBufferMemoryBarrier bar{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;bar.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=dst;bar.size=size;vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,1,&bar,0,nullptr);r=vkEndCommandBuffer(cb);}
 if(r!=VK_SUCCESS){e="Vulkan command recording failed";vkFreeCommandBuffers(device_,command_pool_,1,&cb);return false;}
 VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence f=VK_NULL_HANDLE;r=vkCreateFence(device_,&fi,nullptr,&f);if(r==VK_SUCCESS){VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cb;r=vkQueueSubmit(graphics_queue_,1,&si,f);if(r==VK_SUCCESS)r=vkWaitForFences(device_,1,&f,VK_TRUE,UINT64_MAX);vkDestroyFence(device_,f,nullptr);}vkFreeCommandBuffers(device_,command_pool_,1,&cb);if(r!=VK_SUCCESS){e="Vulkan buffer copy submission failed";return false;}return true;
}
}