#include "viewport.hpp"
#include "gpu_buffer.hpp"
#include "rhizo/editor_geometry.hpp"
#include "rhizo/rhizo.h"
#include "shader_spv.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
namespace rhizo {
namespace {
struct RenderVertex { float p[3],n[3],color[4],material[2]; };
struct DrawPush { float vp[16]; float model[16]; float settings[4]; };
VkShaderModule shader_module(VkDevice d,const uint32_t* code,size_t bytes){
 VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=bytes;ci.pCode=code;
 VkShaderModule m=VK_NULL_HANDLE;return vkCreateShaderModule(d,&ci,nullptr,&m)==VK_SUCCESS?m:VK_NULL_HANDLE;
}
}
Viewport::~Viewport(){detach();}
bool Viewport::attach(VulkanContext&c,ANativeWindow*w,std::string&e){
 if(!w){e="ANativeWindow is null";return false;}detach();context_=&c;window_=w;ANativeWindow_acquire(window_);
 VkAndroidSurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};si.window=window_;
 VkResult r=vkCreateAndroidSurfaceKHR(c.instance(),&si,nullptr,&surface_);
 if(r!=VK_SUCCESS){e="vkCreateAndroidSurfaceKHR failed: "+std::to_string(r);detach();return false;}
 VkBool32 present=VK_FALSE;r=vkGetPhysicalDeviceSurfaceSupportKHR(c.physical_device(),c.graphics_queue_family(),surface_,&present);
 if(r!=VK_SUCCESS||!present){e="Selected Vulkan graphics queue cannot present to Android surface";detach();return false;}
 if(!create_swapchain(e)||!create_pipeline(e)){detach();return false;}
 VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=c.command_pool();ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
 r=vkAllocateCommandBuffers(c.device(),&ai,&command_);if(r!=VK_SUCCESS){e="vkAllocateCommandBuffers(viewport) failed";detach();return false;}
 VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
 if(vkCreateSemaphore(c.device(),&sem,nullptr,&acquired_)!=VK_SUCCESS||vkCreateSemaphore(c.device(),&sem,nullptr,&finished_)!=VK_SUCCESS){e="Viewport semaphore creation failed";detach();return false;}
 VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
 if(vkCreateFence(c.device(),&fi,nullptr,&fence_)!=VK_SUCCESS){e="Viewport fence creation failed";detach();return false;}return true;
}
bool Viewport::create_swapchain(std::string&e){
 VkSurfaceCapabilitiesKHR caps{};VkResult r=vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physical_device(),surface_,&caps);
 if(r!=VK_SUCCESS){e="Surface capability query failed";return false;}
 uint32_t n=0;vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physical_device(),surface_,&n,nullptr);
 if(!n){e="Android surface exposes no Vulkan formats";return false;}
 std::vector<VkSurfaceFormatKHR> fs(n);vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physical_device(),surface_,&n,fs.data());
 VkSurfaceFormatKHR chosen=fs[0];for(const auto&f:fs)if(f.format==VK_FORMAT_B8G8R8A8_UNORM&&f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){chosen=f;break;}format_=chosen.format;
 if(caps.currentExtent.width!=UINT32_MAX)extent_=caps.currentExtent;
 else{extent_.width=std::clamp(static_cast<uint32_t>(ANativeWindow_getWidth(window_)),caps.minImageExtent.width,caps.maxImageExtent.width);extent_.height=std::clamp(static_cast<uint32_t>(ANativeWindow_getHeight(window_)),caps.minImageExtent.height,caps.maxImageExtent.height);}
 uint32_t count=std::max(caps.minImageCount,2u);if(caps.maxImageCount&&count>caps.maxImageCount)count=caps.maxImageCount;
 VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};ci.surface=surface_;ci.minImageCount=count;ci.imageFormat=format_;ci.imageColorSpace=chosen.colorSpace;ci.imageExtent=extent_;ci.imageArrayLayers=1;ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.preTransform=caps.currentTransform;
 ci.compositeAlpha=(caps.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)?VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:static_cast<VkCompositeAlphaFlagBitsKHR>(1u<<__builtin_ctz(caps.supportedCompositeAlpha));ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;ci.clipped=VK_TRUE;
 r=vkCreateSwapchainKHR(context_->device(),&ci,nullptr,&swapchain_);if(r!=VK_SUCCESS){e="vkCreateSwapchainKHR failed: "+std::to_string(r);return false;}
 vkGetSwapchainImagesKHR(context_->device(),swapchain_,&n,nullptr);images_.resize(n);vkGetSwapchainImagesKHR(context_->device(),swapchain_,&n,images_.data());views_.resize(n);framebuffers_.resize(n);
 for(uint32_t i=0;i<n;i++){VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=images_[i];vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format_;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};r=vkCreateImageView(context_->device(),&vi,nullptr,&views_[i]);if(r!=VK_SUCCESS){e="Swapchain image view creation failed";return false;}}
 VkFormat depthCandidates[]={VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D16_UNORM};
 for(VkFormat f:depthCandidates){VkFormatProperties p{};vkGetPhysicalDeviceFormatProperties(context_->physical_device(),f,&p);if(p.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT){depth_format_=f;break;}}
 if(depth_format_==VK_FORMAT_UNDEFINED){e="No supported Vulkan depth attachment format";return false;}
 VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=depth_format_;ii.extent={extent_.width,extent_.height,1};ii.mipLevels=1;ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 r=vkCreateImage(context_->device(),&ii,nullptr,&depth_image_);if(r!=VK_SUCCESS){e="Depth image creation failed: "+std::to_string(r);return false;}
 VkMemoryRequirements mr{};vkGetImageMemoryRequirements(context_->device(),depth_image_,&mr);VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(context_->physical_device(),&mp);uint32_t mt=UINT32_MAX;
 for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){mt=i;break;}
 if(mt==UINT32_MAX){e="No device-local memory for depth image";return false;}
 VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=mr.size;ma.memoryTypeIndex=mt;r=vkAllocateMemory(context_->device(),&ma,nullptr,&depth_memory_);if(r!=VK_SUCCESS){e="Depth memory allocation failed";return false;}
 r=vkBindImageMemory(context_->device(),depth_image_,depth_memory_,0);if(r!=VK_SUCCESS){e="Depth image bind failed";return false;}
 VkImageViewCreateInfo dvi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};dvi.image=depth_image_;dvi.viewType=VK_IMAGE_VIEW_TYPE_2D;dvi.format=depth_format_;dvi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};r=vkCreateImageView(context_->device(),&dvi,nullptr,&depth_view_);if(r!=VK_SUCCESS){e="Depth image view creation failed";return false;}
 VkAttachmentDescription ads[2]{};ads[0].format=format_;ads[0].samples=VK_SAMPLE_COUNT_1_BIT;ads[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ads[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;ads[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;ads[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;ads[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;ads[0].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
 ads[1].format=depth_format_;ads[1].samples=VK_SAMPLE_COUNT_1_BIT;ads[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ads[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;ads[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;ads[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;ads[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;ads[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 VkAttachmentReference cr{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},dr{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&cr;sub.pDepthStencilAttachment=&dr;
 VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;dep.dstStageMask=dep.srcStageMask;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
 VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=2;rp.pAttachments=ads;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;r=vkCreateRenderPass(context_->device(),&rp,nullptr,&render_pass_);if(r!=VK_SUCCESS){e="Viewport render pass creation failed";return false;}
 for(uint32_t i=0;i<n;i++){VkImageView at[]={views_[i],depth_view_};VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=render_pass_;fb.attachmentCount=2;fb.pAttachments=at;fb.width=extent_.width;fb.height=extent_.height;fb.layers=1;r=vkCreateFramebuffer(context_->device(),&fb,nullptr,&framebuffers_[i]);if(r!=VK_SUCCESS){e="Viewport framebuffer creation failed";return false;}}
 return true;
}
bool Viewport::create_pipeline(std::string&e){
 VkDevice d=context_->device();VkShaderModule vs=shader_module(d,shaders::vertex,shaders::vertex_bytes),fs=shader_module(d,shaders::fragment,shaders::fragment_bytes);
 if(!vs||!fs){if(vs)vkDestroyShaderModule(d,vs,nullptr);if(fs)vkDestroyShaderModule(d,fs,nullptr);e="PBR SPIR-V shader module creation failed";return false;}
 VkPushConstantRange range{};range.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;range.offset=0;range.size=sizeof(DrawPush);
 VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;VkResult r=vkCreatePipelineLayout(d,&li,nullptr,&pipeline_layout_);if(r!=VK_SUCCESS){e="Graphics pipeline layout creation failed";vkDestroyShaderModule(d,vs,nullptr);vkDestroyShaderModule(d,fs,nullptr);return false;}
 VkPipelineShaderStageCreateInfo stages[2]{};stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
 VkVertexInputBindingDescription binding{0,sizeof(RenderVertex),VK_VERTEX_INPUT_RATE_VERTEX};
 VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(RenderVertex,p)},{1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(RenderVertex,n)},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(RenderVertex,color)},{3,0,VK_FORMAT_R32G32_SFLOAT,offsetof(RenderVertex,material)}};
 VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=4;vi.pVertexAttributeDescriptions=attrs;
 VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
 VkViewport vp{0,0,static_cast<float>(extent_.width),static_cast<float>(extent_.height),0,1};VkRect2D sc{{0,0},extent_};VkPipelineViewportStateCreateInfo vps{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vps.viewportCount=1;vps.pViewports=&vp;vps.scissorCount=1;vps.pScissors=&sc;
 VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_BACK_BIT;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
 VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
 VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
 VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
 VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
 VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&vps;pi.pRasterizationState=&rs;pi.pMultisampleState=&ms;pi.pDepthStencilState=&ds;pi.pColorBlendState=&bs;pi.layout=pipeline_layout_;pi.renderPass=render_pass_;pi.subpass=0;
 r=vkCreateGraphicsPipelines(d,VK_NULL_HANDLE,1,&pi,nullptr,&triangle_pipeline_);if(r!=VK_SUCCESS){e="PBR graphics pipeline creation failed: "+std::to_string(r);vkDestroyShaderModule(d,vs,nullptr);vkDestroyShaderModule(d,fs,nullptr);destroy_pipeline();return false;}
 ia.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;rs.cullMode=VK_CULL_MODE_NONE;ds.depthTestEnable=VK_FALSE;ds.depthWriteEnable=VK_FALSE;ba.blendEnable=VK_TRUE;ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.alphaBlendOp=VK_BLEND_OP_ADD;
 r=vkCreateGraphicsPipelines(d,VK_NULL_HANDLE,1,&pi,nullptr,&line_pipeline_);if(r!=VK_SUCCESS){e="Editor line pipeline creation failed: "+std::to_string(r);vkDestroyShaderModule(d,vs,nullptr);vkDestroyShaderModule(d,fs,nullptr);destroy_pipeline();return false;}
 ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;rs.cullMode=VK_CULL_MODE_NONE;ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;ba.blendEnable=VK_FALSE;r=vkCreateGraphicsPipelines(d,VK_NULL_HANDLE,1,&pi,nullptr,&triangle_no_cull_pipeline_);if(r!=VK_SUCCESS){e="Unculled PBR pipeline creation failed: "+std::to_string(r);vkDestroyShaderModule(d,vs,nullptr);vkDestroyShaderModule(d,fs,nullptr);destroy_pipeline();return false;}vkDestroyShaderModule(d,vs,nullptr);vkDestroyShaderModule(d,fs,nullptr);return true;
}
bool Viewport::render(float red,float green,float blue,float alpha,std::string&e){
 clear_color_={std::clamp(red,0.0f,1.0f),std::clamp(green,0.0f,1.0f),std::clamp(blue,0.0f,1.0f),std::clamp(alpha,0.0f,1.0f)};
 const Mat4 view=look_at({8.0f,7.0f,10.0f},{0,0,0},{0,1,0});
 const Mat4 projection=perspective(0.785398163f,static_cast<float>(extent_.width)/static_cast<float>(std::max(1u,extent_.height)),0.1f,1000.0f);
 return render_scene(Scene{},multiply(projection,view),e);
}
bool Viewport::render_scene(const Scene&scene,const Mat4&viewProjection,std::string&e){
 if(!context_||!swapchain_||!command_){e="Viewport is not attached";return false;}
 VkDevice d=context_->device();VkResult r=vkWaitForFences(d,1,&fence_,VK_TRUE,UINT64_MAX);if(r!=VK_SUCCESS){e="Viewport fence wait failed";return false;}
 transient_buffers_.clear();
 uint32_t index=0;r=vkAcquireNextImageKHR(d,swapchain_,UINT64_MAX,acquired_,VK_NULL_HANDLE,&index);
 if(r==VK_ERROR_OUT_OF_DATE_KHR){e="Viewport swapchain is out of date; recreate after surface resize";return false;}if(r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR){e="vkAcquireNextImageKHR failed: "+std::to_string(r);return false;}
 vkResetFences(d,1,&fence_);vkResetCommandBuffer(command_,0);VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};r=vkBeginCommandBuffer(command_,&bi);if(r!=VK_SUCCESS){e="Viewport command begin failed";return false;}
 VkClearValue clears[2]{};clears[0].color={{clear_color_[0],clear_color_[1],clear_color_[2],clear_color_[3]}};clears[1].depthStencil={1.0f,0};
 VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=render_pass_;ri.framebuffer=framebuffers_[index];ri.renderArea={{0,0},extent_};ri.clearValueCount=2;ri.pClearValues=clears;vkCmdBeginRenderPass(command_,&ri,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport vp{0,0,static_cast<float>(extent_.width),static_cast<float>(extent_.height),0,1};VkRect2D sc{{0,0},extent_};vkCmdSetViewport(command_,0,1,&vp);vkCmdSetScissor(command_,0,1,&sc);
 auto upload=[&](const void*data,VkDeviceSize size,VkBufferUsageFlags usage)->VkBuffer{
   auto b=std::make_unique<GpuBuffer>();std::string err;if(!b->create(*context_,size,usage,true,err)||!b->write(0,data,size,err)){e=err;return VK_NULL_HANDLE;}VkBuffer h=b->handle();transient_buffers_.push_back(std::move(b));return h;};
 vkCmdBindPipeline(command_,VK_PIPELINE_BIND_POINT_GRAPHICS,(!settings_.enable_backface_culling?triangle_no_cull_pipeline_:triangle_pipeline_));
 mesh_cache_.resize(scene.meshes.size());
 for(const auto&node:scene.nodes()){if(!node.visible||node.mesh_indices.empty())continue;Mat4 model=scene.world_matrix(node.id);for(uint32_t mi:node.mesh_indices){if(mi>=scene.meshes.size())continue;const Mesh&m=scene.meshes[mi];if(m.vertices.empty()||m.indices.empty())continue;
  Material mat{};if(m.material_index<scene.materials.size())mat=scene.materials[m.material_index];
  MeshGpuCache&cache=mesh_cache_[mi];const bool changed=cache.source_vertices!=m.vertices.data()||cache.source_indices!=m.indices.data()||cache.vertex_count!=m.vertices.size()||cache.index_count!=m.indices.size()||cache.base_color.x!=mat.base_color.x||cache.base_color.y!=mat.base_color.y||cache.base_color.z!=mat.base_color.z||cache.base_color.w!=mat.base_color.w||cache.metallic!=mat.metallic||cache.roughness!=mat.roughness;
  if(changed){
   std::vector<RenderVertex> verts;verts.reserve(m.vertices.size());for(const Vertex&v:m.vertices)verts.push_back({{v.position.x,v.position.y,v.position.z},{v.normal.x,v.normal.y,v.normal.z},{mat.base_color.x,mat.base_color.y,mat.base_color.z,mat.base_color.w},{mat.metallic,mat.roughness}});
   const VkDeviceSize vbSize=static_cast<VkDeviceSize>(verts.size()*sizeof(RenderVertex)),ibSize=static_cast<VkDeviceSize>(m.indices.size()*sizeof(uint32_t));std::string err;
   if(!cache.vertices||cache.vertices->size()!=vbSize){cache.vertices=std::make_unique<GpuBuffer>();if(!cache.vertices->create(*context_,vbSize,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true,err)){e=err;vkCmdEndRenderPass(command_);vkEndCommandBuffer(command_);return false;}}
   if(!cache.indices||cache.indices->size()!=ibSize){cache.indices=std::make_unique<GpuBuffer>();if(!cache.indices->create(*context_,ibSize,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,true,err)){e=err;vkCmdEndRenderPass(command_);vkEndCommandBuffer(command_);return false;}}
   if(!cache.vertices->write(0,verts.data(),vbSize,err)||!cache.indices->write(0,m.indices.data(),ibSize,err)){e=err;vkCmdEndRenderPass(command_);vkEndCommandBuffer(command_);return false;}
   cache.source_vertices=m.vertices.data();cache.source_indices=m.indices.data();cache.vertex_count=m.vertices.size();cache.index_count=m.indices.size();cache.base_color=mat.base_color;cache.metallic=mat.metallic;cache.roughness=mat.roughness;
  }
  VkBuffer vb=cache.vertices->handle(),ib=cache.indices->handle();VkDeviceSize off=0;vkCmdBindVertexBuffers(command_,0,1,&vb,&off);vkCmdBindIndexBuffer(command_,ib,0,VK_INDEX_TYPE_UINT32);DrawPush push{};std::memcpy(push.vp,viewProjection.data(),64);std::memcpy(push.model,model.data(),64);push.settings[0]=settings_.exposure;push.settings[1]=settings_.environment_intensity;push.settings[2]=settings_.enable_pbr?1.0f:0.0f;push.settings[3]=settings_.enable_textures?1.0f:0.0f;vkCmdPushConstants(command_,pipeline_layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,static_cast<uint32_t>(sizeof(push)),&push);vkCmdDrawIndexed(command_,static_cast<uint32_t>(m.indices.size()),1,0,0,0);
 }}
 // Editor grid and origin gizmo are real line geometry in the same depth-aware viewport.
 auto drawLines=[&](const EditorGeometry&g,const Mat4&model){if(g.vertices.empty())return;std::vector<RenderVertex> v;v.reserve(g.vertices.size());for(const auto&x:g.vertices)v.push_back({{x.position.x,x.position.y,x.position.z},{0,0,1},{x.color.x,x.color.y,x.color.z,x.color.w},{0,1}});VkBuffer b=upload(v.data(),static_cast<VkDeviceSize>(v.size()*sizeof(RenderVertex)),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);if(!b)return;vkCmdBindPipeline(command_,VK_PIPELINE_BIND_POINT_GRAPHICS,line_pipeline_);VkDeviceSize off=0;vkCmdBindVertexBuffers(command_,0,1,&b,&off);DrawPush push{};std::memcpy(push.vp,viewProjection.data(),64);std::memcpy(push.model,model.data(),64);push.settings[0]=settings_.exposure;push.settings[1]=settings_.environment_intensity;push.settings[2]=settings_.enable_pbr?1.0f:0.0f;push.settings[3]=settings_.enable_textures?1.0f:0.0f;vkCmdPushConstants(command_,pipeline_layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);vkCmdDraw(command_,static_cast<uint32_t>(v.size()),1,0,0);};
 drawLines(make_grid(10.0f,20),Mat4::identity());drawLines(make_transform_gizmo({0,0,0},1.25f),Mat4::identity());
 vkCmdEndRenderPass(command_);r=vkEndCommandBuffer(command_);if(r!=VK_SUCCESS){e="Viewport command end failed";return false;}
 VkPipelineStageFlags wait=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.waitSemaphoreCount=1;si.pWaitSemaphores=&acquired_;si.pWaitDstStageMask=&wait;si.commandBufferCount=1;si.pCommandBuffers=&command_;si.signalSemaphoreCount=1;si.pSignalSemaphores=&finished_;r=vkQueueSubmit(context_->graphics_queue(),1,&si,fence_);if(r!=VK_SUCCESS){e="Viewport queue submit failed: "+std::to_string(r);return false;}
 VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};pi.waitSemaphoreCount=1;pi.pWaitSemaphores=&finished_;pi.swapchainCount=1;pi.pSwapchains=&swapchain_;pi.pImageIndices=&index;r=vkQueuePresentKHR(context_->graphics_queue(),&pi);if(r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR){e="vkQueuePresentKHR failed: "+std::to_string(r);return false;}return true;
}
void Viewport::destroy_pipeline()noexcept{if(!context_)return;VkDevice d=context_->device();if(triangle_pipeline_)vkDestroyPipeline(d,triangle_pipeline_,nullptr);if(triangle_no_cull_pipeline_)vkDestroyPipeline(d,triangle_no_cull_pipeline_,nullptr);if(line_pipeline_)vkDestroyPipeline(d,line_pipeline_,nullptr);if(pipeline_layout_)vkDestroyPipelineLayout(d,pipeline_layout_,nullptr);triangle_pipeline_=triangle_no_cull_pipeline_=line_pipeline_=VK_NULL_HANDLE;pipeline_layout_=VK_NULL_HANDLE;}
void Viewport::destroy_swapchain()noexcept{if(!context_)return;VkDevice d=context_->device();for(auto f:framebuffers_)if(f)vkDestroyFramebuffer(d,f,nullptr);framebuffers_.clear();if(render_pass_)vkDestroyRenderPass(d,render_pass_,nullptr);render_pass_=VK_NULL_HANDLE;for(auto v:views_)if(v)vkDestroyImageView(d,v,nullptr);views_.clear();images_.clear();if(depth_view_)vkDestroyImageView(d,depth_view_,nullptr);if(depth_image_)vkDestroyImage(d,depth_image_,nullptr);if(depth_memory_)vkFreeMemory(d,depth_memory_,nullptr);depth_view_=VK_NULL_HANDLE;depth_image_=VK_NULL_HANDLE;depth_memory_=VK_NULL_HANDLE;if(swapchain_)vkDestroySwapchainKHR(d,swapchain_,nullptr);swapchain_=VK_NULL_HANDLE;}
void Viewport::detach()noexcept{if(context_){VkDevice d=context_->device();if(d)vkDeviceWaitIdle(d);transient_buffers_.clear();mesh_cache_.clear();destroy_pipeline();destroy_swapchain();if(command_)vkFreeCommandBuffers(d,context_->command_pool(),1,&command_);if(acquired_)vkDestroySemaphore(d,acquired_,nullptr);if(finished_)vkDestroySemaphore(d,finished_,nullptr);if(fence_)vkDestroyFence(d,fence_,nullptr);if(surface_)vkDestroySurfaceKHR(context_->instance(),surface_,nullptr);}command_=VK_NULL_HANDLE;acquired_=finished_=VK_NULL_HANDLE;fence_=VK_NULL_HANDLE;surface_=VK_NULL_HANDLE;if(window_){ANativeWindow_release(window_);window_=nullptr;}context_=nullptr;extent_={};}
}