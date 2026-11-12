#pragma once
#include "rhizo/scene.hpp"
#include <string>
namespace rhizo {
struct ImportReport { uint32_t nodes=0,meshes=0,materials=0; std::string model_path; };
bool import_model(Scene& scene,const std::string& path,const std::string& cache_directory,
                  ImportReport& report,std::string& error);
}
