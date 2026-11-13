#pragma once
#include "rhizo/scene.hpp"
#include <string>
namespace rhizo {
bool save_scene(const Scene&,const std::string&,std::string&);
bool load_scene(Scene&,const std::string&,std::string&);
bool save_prefab(const Scene&,EntityId,const std::string&,std::string&);
bool instantiate_prefab(Scene&,const std::string&,EntityId,EntityId&,std::string&);
}
