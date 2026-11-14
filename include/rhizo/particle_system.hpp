#pragma once
#include "rhizo/scene.hpp"
#include "rhizo/rhizo.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace rhizo {
class ParticleSystem {
 public:
  using RaycastFn=bool(*)(void*,const float*,const float*,RhizoRaycastHit&);
  bool set_emitter(Scene&,EntityId,const RhizoParticleEmitterInfo&);
  bool remove_emitter(Scene&,EntityId);
  bool burst(const Scene&,EntityId,uint32_t);
  bool step(Scene&,double,void*,RaycastFn);
  bool rebuild(Scene&);
  uint32_t count(EntityId)const;
  uint32_t copy(EntityId,RhizoParticle*,uint32_t)const;
  void clear();
 private:
  struct Particle { Vec3 position{},velocity{};float age=0,lifetime=1,size=1;Vec4 color{}; };
  struct Emitter { RhizoParticleEmitterInfo info{};std::vector<Particle> particles;double accumulator=0;uint32_t random=1; };
  bool spawn(Emitter&,Vec3);
  static float random01(uint32_t&);
  std::unordered_map<EntityId,Emitter> emitters_;
};
}
