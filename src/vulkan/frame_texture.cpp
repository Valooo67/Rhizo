#include "frame_texture.hpp"
#include <array>
namespace rhizo {
FrameTexture::~FrameTexture(){destroy();}
bool FrameTexture::create(VulkanContext&c,uint32_t w,uint32_t h,VkFormat f,std::string&e){
 destroy();context_=&c;format_=f;width_=w;height_=h;VkDevice d=c.device();
 VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=f;ii.extent={w,h,1};ii.mipLevels=1;ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 VkResult r=vkCreateImage(d,&ii,nullptr,&image_);if(r!=VK_SUCCESS){e="OpenGL frame VkImage creation failed: "+std::to_string(r);destroy();return false;}
 VkMemoryRequirements mr{};vkGetImageMemoryRequirements(d,image_,&mr);VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(c.physical_device(),&mp);uint32_t mt=UINT32_MAX;
 for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){mt=i;break;}
 if(mt==UINT32_MAX){e="No device-local memory for OpenGL frame texture";destroy();return false;}
 VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=mr.size;ma.memoryTypeIndex=mt;r=vkAllocateMemory(d,&ma,nullptr,&memory_);if(r!=VK_SUCCESS){e="OpenGL frame image memory allocation failed";destroy();return false;}
 r=vkBindImageMemory(d,image_,memory_,0);if(r!=VK_SUCCESS){e="OpenGL frame image memory bind failed";destroy();return false;}
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image_;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=f;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};r=vkCreateImageView(d,&vi,nullptr,&view_);if(r!=VK_SUCCESS){e="OpenGL frame image view creation failed";destroy();return false;}
 VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=VK_FILTER_LINEAR;si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.maxLod=0;si.anisotropyEnable=VK_FALSE;r=vkCreateSampler(d,&si,nullptr,&sampler_);if(r!=VK_SUCCESS){e="OpenGL frame sampler creation failed";destroy();return false;}return true;
}
bool FrameTexture::upload(VulkanContext&c,VkBuffer src,uint32_t w,uint32_t h,uint32_t stride,bool bgra,std::string&e){
 if(!src||!w||!h||stride<w*4u){e="Invalid OpenGL frame texture upload";return false;}
 VkFormat f=bgra?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_R8G8B8A8_UNORM;
 if(!image_||width_!=w||height_!=h||format_!=f)if(!create(c,w,h,f,e))return false;
 VkDevice d=c.device();VkCommandBuffer cb=VK_NULL_HANDLE;VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=c.command_pool();ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
 VkResult r=vkAllocateCommandBuffers(d,&ai,&cb);if(r!=VK_SUCCESS){e="OpenGL frame command allocation failed";return false;}
 VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;r=vkBeginCommandBuffer(cb,&bi);
 if(r==VK_SUCCESS){
  VkImageMemoryBarrier toCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};toCopy.srcAccessMask=0;toCopy.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;toCopy.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;toCopy.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;toCopy.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;toCopy.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;toCopy.image=image_;toCopy.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&toCopy);
  VkBufferImageCopy cp{};cp.bufferRowLength=stride/4u;cp.bufferImageHeight=h;cp.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};cp.imageExtent={w,h,1};vkCmdCopyBufferToImage(cb,src,image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp);
  VkImageMemoryBarrier toSample{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};toSample.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;toSample.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;toSample.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;toSample.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;toSample.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;toSample.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;toSample.image=image_;toSample.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&toSample);
  r=vkEndCommandBuffer(cb);
 }
 if(r==VK_SUCCESS){VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cb;r=vkQueueSubmit(c.graphics_queue(),1,&si,VK_NULL_HANDLE);if(r==VK_SUCCESS)r=vkQueueWaitIdle(c.graphics_queue());}
 vkFreeCommandBuffers(d,c.command_pool(),1,&cb);if(r!=VK_SUCCESS){e="OpenGL frame upload submission failed: "+std::to_string(r);return false;}return true;
}
void FrameTexture::destroy()noexcept{
 if(context_&&context_->device()){VkDevice d=context_->device();if(sampler_)vkDestroySampler(d,sampler_,nullptr);if(view_)vkDestroyImageView(d,view_,nullptr);if(image_)vkDestroyImage(d,image_,nullptr);if(memory_)vkFreeMemory(d,memory_,nullptr);}
 context_=nullptr;image_=VK_NULL_HANDLE;memory_=VK_NULL_HANDLE;view_=VK_NULL_HANDLE;sampler_=VK_NULL_HANDLE;format_=VK_FORMAT_UNDEFINED;width_=height_=0;
}
}
