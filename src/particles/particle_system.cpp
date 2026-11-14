#include "rhizo/particle_system.hpp"
#include "rhizo/components.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace rhizo { namespace {
constexpr uint32_t kEmitterComponent=0x5052544cu;
bool finite3p(const float*p){return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);}
Vec3 array3(const float*p){return {p[0],p[1],p[2]};}
}
float ParticleSystem::random01(uint32_t&s){s^=s<<13;s^=s>>17;s^=s<<5;return static_cast<float>(s&0x00ffffffu)/16777215.0f;}
bool ParticleSystem::set_emitter(Scene&scene,EntityId id,const RhizoParticleEmitterInfo&i){
 if(!scene.get(id)||i.struct_size<sizeof(RhizoParticleEmitterInfo)||i.version!=1||i.capacity==0||i.capacity>100000||i.shape>2||!std::isfinite(i.emission_rate)||i.emission_rate<0||i.emission_rate>100000||!std::isfinite(i.lifetime_min)||!std::isfinite(i.lifetime_max)||i.lifetime_min<=0||i.lifetime_max<i.lifetime_min||!std::isfinite(i.speed_min)||!std::isfinite(i.speed_max)||i.speed_min<0||i.speed_max<i.speed_min||!std::isfinite(i.size_start)||!std::isfinite(i.size_end)||i.size_start<0||i.size_end<0||!finite3p(i.gravity)||!finite3p(i.force)||!finite3p(i.direction)||!finite3p(i.extents)||!std::isfinite(i.cone_angle)||i.cone_angle<0||i.cone_angle>3.14159f||!std::isfinite(i.drag)||i.drag<0||!std::isfinite(i.restitution)||i.restitution<0||i.restitution>1)return false;
 for(float c:i.color_start)if(!std::isfinite(c))return false;for(float c:i.color_end)if(!std::isfinite(c))return false;
 Emitter e;e.info=i;e.random=i.seed?i.seed:(id*747796405u+2891336453u);e.particles.reserve(i.capacity);emitters_[id]=std::move(e);
 auto*set=scene.components.get<CustomComponentSet>(id);if(!set)set=&scene.components.emplace<CustomComponentSet>(id);auto&bytes=set->values[kEmitterComponent];bytes.resize(sizeof(i));std::memcpy(bytes.data(),&i,sizeof(i));return true;
}
bool ParticleSystem::remove_emitter(Scene&scene,EntityId id){auto i=emitters_.erase(id);auto*set=scene.components.get<CustomComponentSet>(id);if(set){set->values.erase(kEmitterComponent);if(set->values.empty())scene.components.remove<CustomComponentSet>(id);}return i!=0;}
bool ParticleSystem::spawn(Emitter&e,Vec3 origin){
 if(e.particles.size()>=e.info.capacity)return false;const auto&i=e.info;Particle p;p.position=origin;
 Vec3 dir=array3(i.direction);const float dlen=length(dir);dir=dlen>1e-6f?dir/dlen:Vec3{0,1,0};
 if(i.shape==1){const float z=random01(e.random)*2-1,a=random01(e.random)*6.2831853f,r=std::cbrt(random01(e.random));const float q=std::sqrt(std::max(0.0f,1-z*z));p.position=p.position+Vec3{q*std::cos(a),z,q*std::sin(a)}*r*std::max({i.extents[0],i.extents[1],i.extents[2]});}
 else if(i.shape==2){p.position=p.position+Vec3{(random01(e.random)*2-1)*i.extents[0],(random01(e.random)*2-1)*i.extents[1],(random01(e.random)*2-1)*i.extents[2]};}
 const float angle=random01(e.random)*i.cone_angle,az=random01(e.random)*6.2831853f;Vec3 right=normalize(cross(dir,std::fabs(dir.y)<0.95f?Vec3{0,1,0}:Vec3{1,0,0})),up=normalize(cross(right,dir));const Vec3 spread=right*(std::sin(angle)*std::cos(az))+up*(std::sin(angle)*std::sin(az))+dir*std::cos(angle);const float speed=i.speed_min+(i.speed_max-i.speed_min)*random01(e.random);p.velocity=spread*speed;p.lifetime=i.lifetime_min+(i.lifetime_max-i.lifetime_min)*random01(e.random);p.size=i.size_start;p.color={i.color_start[0],i.color_start[1],i.color_start[2],i.color_start[3]};e.particles.push_back(p);return true;
}
bool ParticleSystem::burst(const Scene&scene,EntityId id,uint32_t n){auto it=emitters_.find(id);const auto*node=scene.get(id);if(it==emitters_.end()||!node||n>100000)return false;const Mat4 world=scene.world_matrix(id);const Vec3 origin{world.v[12],world.v[13],world.v[14]};for(uint32_t k=0;k<n;k++)if(!spawn(it->second,origin))break;return true;}
bool ParticleSystem::step(Scene&scene,double dt,void*user,RaycastFn raycast){
 if(!std::isfinite(dt)||dt<0||dt>0.25)return false;
 for(auto it=emitters_.begin();it!=emitters_.end();){auto*n=scene.get(it->first);if(!n){it=emitters_.erase(it);continue;}auto&e=it->second;const auto&i=e.info;const Mat4 world=scene.world_matrix(it->first);const Vec3 origin{world.v[12],world.v[13],world.v[14]};
  if(i.enabled){e.accumulator+=static_cast<double>(i.emission_rate)*dt;uint32_t amount=static_cast<uint32_t>(std::min(e.accumulator,static_cast<double>(i.capacity)));e.accumulator-=amount;for(uint32_t k=0;k<amount;k++)spawn(e,origin);}
  const Vec3 gravity=array3(i.gravity)+array3(i.force);for(auto p=e.particles.begin();p!=e.particles.end();){p->age+=static_cast<float>(dt);if(p->age>=p->lifetime){p=e.particles.erase(p);continue;}const float life=std::clamp(p->age/p->lifetime,0.0f,1.0f);p->velocity=p->velocity+(gravity*static_cast<float>(dt));p->velocity=p->velocity*std::max(0.0f,1.0f-i.drag*static_cast<float>(dt));const Vec3 next=p->position+p->velocity*static_cast<float>(dt);
   if(i.collision_enabled&&raycast){const float from[3]={p->position.x,p->position.y,p->position.z},to[3]={next.x,next.y,next.z};RhizoRaycastHit hit{};if(raycast(user,from,to,hit)){Vec3 normal{hit.normal[0],hit.normal[1],hit.normal[2]};normal=normalize(normal);const float vn=dot(p->velocity,normal);if(vn<0)p->velocity=p->velocity-normal*((1.0f+i.restitution)*vn);p->position={hit.point[0]+normal.x*0.002f,hit.point[1]+normal.y*0.002f,hit.point[2]+normal.z*0.002f};}else p->position=next;}else p->position=next;
   p->size=i.size_start+(i.size_end-i.size_start)*life;p->color={i.color_start[0]+(i.color_end[0]-i.color_start[0])*life,i.color_start[1]+(i.color_end[1]-i.color_start[1])*life,i.color_start[2]+(i.color_end[2]-i.color_start[2])*life,i.color_start[3]+(i.color_end[3]-i.color_start[3])*life};++p;
  }++it;
 }
 return true;
}
bool ParticleSystem::rebuild(Scene&scene){clear();for(const auto&n:scene.nodes()){const auto*set=scene.components.get<CustomComponentSet>(n.id);if(!set)continue;auto i=set->values.find(kEmitterComponent);if(i==set->values.end())continue;if(i->second.size()!=sizeof(RhizoParticleEmitterInfo))return false;RhizoParticleEmitterInfo d{};std::memcpy(&d,i->second.data(),sizeof(d));if(!set_emitter(scene,n.id,d))return false;}return true;}
uint32_t ParticleSystem::count(EntityId id)const{auto i=emitters_.find(id);return i==emitters_.end()?0u:static_cast<uint32_t>(i->second.particles.size());}
uint32_t ParticleSystem::copy(EntityId id,RhizoParticle*out,uint32_t capacity)const{auto i=emitters_.find(id);if(i==emitters_.end()||!out)return 0;const auto&v=i->second.particles;const uint32_t n=std::min(capacity,static_cast<uint32_t>(v.size()));for(uint32_t k=0;k<n;k++){const auto&p=v[k];auto&o=out[k];o.entity_id=id;o.position[0]=p.position.x;o.position[1]=p.position.y;o.position[2]=p.position.z;o.velocity[0]=p.velocity.x;o.velocity[1]=p.velocity.y;o.velocity[2]=p.velocity.z;o.age=p.age;o.lifetime=p.lifetime;o.size=p.size;o.color[0]=p.color.x;o.color[1]=p.color.y;o.color[2]=p.color.z;o.color[3]=p.color.w;}return n;}
void ParticleSystem::clear(){emitters_.clear();}
}
