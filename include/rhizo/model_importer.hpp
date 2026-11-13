#pragma once
#include "rhizo/animation.hpp"
#include <string>
#include <vector>
namespace rhizo {
struct ImportReport { uint32_t nodes=0,meshes=0,materials=0,animations=0,bones=0,morph_targets=0; std::string model_path; };
bool import_model(Scene& scene,const std::string& path,const std::string& cache_directory,
                  ImportReport& report,std::string& error,std::vector<AnimationClip>* animations=nullptr);
}
