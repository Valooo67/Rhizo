#pragma once
#include "rhizo/math.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace rhizo {
using EntityId=uint32_t;
constexpr EntityId kInvalidEntity=0;
struct Transform { Vec3 position{}; Quat rotation{}; Vec3 scale{1,1,1}; };
struct Material {
  Vec4 base_color{0.8f,0.8f,0.8f,1};
  float metallic=0, roughness=0.7f, normal_scale=1, occlusion=1;
  std::string base_color_texture, normal_texture, metallic_roughness_texture, emissive_texture;
};
struct Vertex { Vec3 position,normal; Vec2 uv; Vec4 tangent{1,0,0,1}; };
struct Mesh {
  std::string name;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  uint32_t material_index=0;
};
struct SceneNode {
  EntityId id=kInvalidEntity,parent=kInvalidEntity;
  std::string name;
  Transform local;
  std::vector<EntityId> children;
  std::vector<uint32_t> mesh_indices;
  bool visible=true;
};
class Scene {
 public:
  EntityId create(std::string name,EntityId parent=kInvalidEntity);
  bool destroy(EntityId id);
  SceneNode* get(EntityId id);
  const SceneNode* get(EntityId id) const;
  bool set_parent(EntityId id,EntityId parent);
  Mat4 world_matrix(EntityId id) const;
  std::vector<EntityId> roots() const;
  std::vector<SceneNode>& nodes(){return nodes_;}
  const std::vector<SceneNode>& nodes()const{return nodes_;}
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
 private:
  std::vector<SceneNode> nodes_;
  EntityId next_id_=1;
};
}
