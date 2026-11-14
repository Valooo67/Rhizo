#pragma once
#include "rhizo/rhizo.h"
#include "rhizo/scene.hpp"
#include <memory>
#include <string>
namespace rhizo {
class PhysicsWorld {
 public:
  PhysicsWorld();
  ~PhysicsWorld();
  bool create_body(Scene&, EntityId, const RhizoRigidBodyInfo&, std::string&);
  bool remove_body(Scene&, EntityId);
  bool create_joint(Scene&, EntityId, const RhizoPhysicsJointInfo&, std::string&);
  bool remove_joint(Scene&, EntityId);
  bool set_animation_mode(Scene&, EntityId, const RhizoPhysicsAnimationInfo&);
  bool set_velocity(EntityId, const float*, const float*);
  bool apply_force(EntityId, const float*, bool);
  bool raycast(const float*, const float*, RhizoRaycastHit&) const;
  bool step(Scene&, double, std::string&);
  void clear();
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}