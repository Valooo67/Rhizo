#pragma once
#include "rhizo/scene.hpp"\n#include "rhizo/animation.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rhizo {
enum class AssetType:uint32_t { Unknown=0,Texture=1,Model=2,Scene=3,Prefab=4,Audio=5,Shader=6 };
struct TextureResource { uint32_t width=0,height=0;std::vector<uint8_t> rgba;std::string source; };
struct ModelResource { Scene scene;std::vector<AnimationClip> animations;std::string source; };
struct AssetRecord { uint64_t id=0;AssetType type=AssetType::Unknown;std::string path;uint64_t size=0,modified=0; };
class AssetManager {
 public:
  bool scan(const std::string& root,std::string& error);
  const AssetRecord* find(uint64_t id)const;
  const AssetRecord* find_path(const std::string& path)const;
  const std::vector<AssetRecord>& assets()const{return assets_;}
  void clear(){assets_.clear();by_id_.clear();by_path_.clear();}
 private:
  std::vector<AssetRecord> assets_;
  std::unordered_map<uint64_t,size_t> by_id_;
  std::unordered_map<std::string,size_t> by_path_;
};
class ResourceManager {
 public:
  std::shared_ptr<const TextureResource> load_texture(const std::string&,std::string&);
  std::shared_ptr<const ModelResource> load_model(const std::string&,const std::string&,std::string&);
  void collect_unused();
  void clear();
  size_t texture_cache_size()const{return textures_.size();}
  size_t model_cache_size()const{return models_.size();}
 private:
  std::unordered_map<std::string,std::shared_ptr<const TextureResource>> textures_;
  std::unordered_map<std::string,std::shared_ptr<const ModelResource>> models_;
};
}
