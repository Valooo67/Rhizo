#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "image_decode.hpp"
#include <limits>
namespace rhizo { bool decode_image_rgba8(const std::string& path,std::vector<uint8_t>& pixels,uint32_t& width,uint32_t& height,std::string& error){int w=0,h=0,channels=0;unsigned char* data=stbi_load(path.c_str(),&w,&h,&channels,4);if(!data){error=std::string("Texture decode failed: ")+stbi_failure_reason();return false;}if(w<=0||h<=0||w>16384||h>16384||uint64_t(w)*uint64_t(h)>uint64_t(std::numeric_limits<size_t>::max()/4)){stbi_image_free(data);error="Texture dimensions are invalid or too large";return false;}try{pixels.assign(data,data+size_t(w)*size_t(h)*4);}catch(...){stbi_image_free(data);error="Texture pixel allocation failed";return false;}stbi_image_free(data);width=uint32_t(w);height=uint32_t(h);return true;} }
