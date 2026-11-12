cmake_minimum_required(VERSION 3.22)
project(RHIZO VERSION 0.2.0 LANGUAGES C CXX)
if(NOT ANDROID)
  message(FATAL_ERROR "RHIZO runtime is Android ARM64 only. Configure with the Android NDK.")
endif()
if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "RHIZO supports arm64-v8a only; received: ${ANDROID_ABI}")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.4.3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(assimp)
find_library(ANDROID_VULKAN_LIBRARY vulkan REQUIRED)
find_library(ANDROID_LOG_LIBRARY log)
find_library(ANDROID_Z_LIBRARY z REQUIRED)
add_library(rhizo SHARED
  src/core/rhizo.cpp
  src/vulkan/vulkan_context.cpp
  src/vulkan/gpu_buffer.cpp
  src/vulkan/shadow_map.cpp
  src/scene/scene.cpp
  src/scene/animation.cpp
  src/scene/model_importer.cpp
  src/scene/editor_geometry.cpp)
target_include_directories(rhizo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rhizo PRIVATE ${ANDROID_VULKAN_LIBRARY} ${ANDROID_Z_LIBRARY} assimp)
if(ANDROID_LOG_LIBRARY)
  target_link_libraries(rhizo PRIVATE ${ANDROID_LOG_LIBRARY})
endif()
target_compile_options(rhizo PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wno-unused-parameter)
target_compile_definitions(rhizo PRIVATE VK_USE_PLATFORM_ANDROID_KHR=1 RHIZO_BUILDING_LIBRARY=1)
