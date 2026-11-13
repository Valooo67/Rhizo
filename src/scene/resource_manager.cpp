#include "rhizo/resource_manager.hpp"
#include "rhizo/model_importer.hpp"
#include "../vulkan/image_decode.hpp"
#include <algorithm>
#include <chrono>\n#include <cctype>
#include <filesystem>
#include <system_error>

namespace rhizo { namespace fs=std::filesystem; namespace {
std::string normalized(const std::string&p){std::error_code ec;auto x=fs::weakly_canonical(fs::path(p),ec);return (ec?fs::path(p).lexically_normal():x).string();}
uint64_t hash_path(const std::string&s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h?h:1;}
AssetType classify(const fs::path&p){std::string e=p.extension().string();std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return char(std::tolower(c));});if(e==".png"||e==".jpg"||e==".jpeg"||e==".webp"||e==".tga"||e==".bmp")return AssetType::Texture;if(e==".fbx"||e==".glb"||e==".gltf"||e==".obj"||e==".dae"||e==".3ds")return AssetType::Model;if(e==".rhizoscene"||e==".scene")return AssetType::Scene;if(e==".rhizoprefab"||e==".prefab")return AssetType::Prefab;if(e==".wav"||e==".ogg"||e==".mp3"||e==".flac")return AssetType::Audio;if(e==".vert"||e==".frag"||e==".glsl"||e==".spv")return AssetType::Shader;return AssetType::Unknown;}
}
bool AssetManager::scan(const std::string&root,std::string&error){
 fs::path dir(root);std::error_code ec;if(!fs::exists(dir,ec)||!fs::is_directory(dir,ec)){error="Asset root is not a directory";return false;}
 std::vector<AssetRecord> next;fs::recursive_directory_iterator it(dir,fs::directory_options::skip_permission_denied,ec),end;
 for(;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(!it->is_regular_file(ec)||ec){ec.clear();continue;}const std::string path=normalized(it->path().string());AssetRecord a;a.path=path;a.id=hash_path(path);a.type=classify(it->path());a.size=it->file_size(ec);if(ec){ec.clear();a.size=0;}auto t=it->last_write_time(ec);if(!ec)a.modified=static_cast<uint64_t>(t.time_since_epoch().count());else ec.clear();next.push_back(std::move(a));}
 std::sort(next.begin(),next.end(),[](const auto&a,const auto&b){return a.path<b.path;});std::unordered_map<uint64_t,size_t> ids;std::unordered_map<std::string,size_t> paths;
 for(size_t i=0;i<next.size();i++){if(ids.contains(next[i].id)){error="Asset ID hash collision detected";return false;}ids[next[i].id]=i;paths[next[i].path]=i;}
 assets_=std::move(next);by_id_=std::move(ids);by_path_=std::move(paths);return true;
}
const AssetRecord*AssetManager::find(uint64_t id)const{auto i=by_id_.find(id);return i==by_id_.end()?nullptr:&assets_[i->second];}
const AssetRecord*AssetManager::find_path(const std::string&p)const{auto i=by_path_.find(normalized(p));return i==by_path_.end()?nullptr:&assets_[i->second];}
std::shared_ptr<const TextureResource>ResourceManager::load_texture(const std::string&path,std::string&error){
 const std::string key=normalized(path);auto i=textures_.find(key);if(i!=textures_.end())return i->second;
 auto value=std::make_shared<TextureResource>();value->source=key;if(!decode_image_rgba8(key,value->rgba,value->width,value->height,error))return {};
 textures_[key]=value;return value;
}
std::shared_ptr<const ModelResource>ResourceManager::load_model(const std::string&path,const std::string&cache,std::string&error){
 const std::string key=normalized(path);auto i=models_.find(key);if(i!=models_.end())return i->second;
 auto value=std::make_shared<ModelResource>();value->source=key;ImportReport report;if(!import_model(value->scene,key,cache,report,error))return {};
 models_[key]=value;return value;
}
void ResourceManager::collect_unused(){for(auto i=textures_.begin();i!=textures_.end();)if(i->second.use_count()==1)i=textures_.erase(i);else ++i;for(auto i=models_.begin();i!=models_.end();)if(i->second.use_count()==1)i=models_.erase(i);else ++i;}
void ResourceManager::clear(){textures_.clear();models_.clear();}
}
