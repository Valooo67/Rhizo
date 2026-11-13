#pragma once
#include "rhizo/math.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "rhizo/components.hpp"

namespace rhizo {
using EntityId=uint32_t;
constexpr EntityId kInvalidEntity=0;
struct Transform { Vec3 position{}; Quat rotation{}; Vec3 scale{1,1,1}; };
struct CameraComponent { float vertical_fov=0.785398163f, near_plane=0.1f, far_plane=1000.0f, orthographic_size=10.0f; bool orthographic=false; };
enum class LightType:uint32_t { Directional=0, Point=1, Spot=2 };
struct LightComponent { LightType type=LightType::Directional; Vec3 color{1,1,1}; float intensity=1.0f, range=10.0f, inner_cone=0.35f, outer_cone=0.6f; bool casts_shadows=true; };
struct Material {
  Vec4 base_color{0.8f,0.8f,0.8f,1};
  float metallic=0, roughness=0.7f, normal_scale=1, occlusion=1;
  Vec3 emissive_color{0,0,0};
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
  bool has_camera=false,has_light=false;
  CameraComponent camera;
  LightComponent light;
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
  void clear();
  ComponentRegistry components;
  std::vector<SceneNode>& nodes(){return nodes_;}
  const std::vector<SceneNode>& nodes()const{return nodes_;}
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
 private:
  std::vector<SceneNode> nodes_;
  EntityId next_id_=1;
};
}
