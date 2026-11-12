#include "rhizo/animation.hpp"
#include <algorithm>
namespace rhizo {
bool Animation::set_key(EntityId id,TrackProperty p,double t,const Transform&v){
  if(id==kInvalidEntity||!std::isfinite(t)||t<0)return false;
  auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});
  if(it==tracks_.end()){tracks_.push_back({id,p,{}});it=std::prev(tracks_.end());}
  it->keys[t]={t,v};duration_=std::max(duration_,t);return true;
}
bool Animation::set_parameter_key(EntityId id,uint32_t property,double t,Vec4 value){
  if(id==kInvalidEntity||property<3||property>11||!std::isfinite(t)||t<0||!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||!std::isfinite(value.w))return false;
  auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==property;});
  if(it==parameter_tracks_.end()){parameter_tracks_.push_back({id,property,{}});it=std::prev(parameter_tracks_.end());}
  it->keys[t]={t,value};duration_=std::max(duration_,t);return true;
}
bool Animation::remove_key(EntityId id,TrackProperty p,double t){
  auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});
  return it!=tracks_.end()&&it->keys.erase(t)>0;
}
bool Animation::evaluate(Scene&scene,double time)const{
  if(!std::isfinite(time))return false;
  for(const auto&track:tracks_){
    auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;
    auto hi=track.keys.lower_bound(time);const Transform* a=nullptr;const Transform*b=nullptr;float t=0;
    if(hi==track.keys.begin())a=b=&hi->second.value;
    else if(hi==track.keys.end())a=b=&std::prev(hi)->second.value;
    else{auto lo=std::prev(hi);a=&lo->second.value;b=&hi->second.value;const double span=hi->first-lo->first;t=span>0?static_cast<float>((time-lo->first)/span):0;}
    if(track.property==TrackProperty::Position)n->local.position=a->position+(b->position-a->position)*t;
    else if(track.property==TrackProperty::Scale)n->local.scale=a->scale+(b->scale-a->scale)*t;
    else n->local.rotation=slerp(a->rotation,b->rotation,t);
  }
  for(const auto& track:parameter_tracks_){
    auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;
    auto hi=track.keys.lower_bound(time);Vec4 a{},b{};float t=0;
    if(hi==track.keys.begin())a=b=hi->second.value;
    else if(hi==track.keys.end())a=b=std::prev(hi)->second.value;
    else{auto lo=std::prev(hi);a=lo->second.value;b=hi->second.value;const double span=hi->first-lo->first;t=span>0?static_cast<float>((time-lo->first)/span):0;}
    const Vec4 v{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
    switch(track.property){
      case 3: if(n->has_camera)n->camera.vertical_fov=v.x; break;
      case 4: if(n->has_camera)n->camera.near_plane=v.x; break;
      case 5: if(n->has_camera)n->camera.far_plane=v.x; break;
      case 6: if(n->has_camera)n->camera.orthographic_size=v.x; break;
      case 7: if(n->has_light)n->light.intensity=v.x; break;
      case 8: if(n->has_light)n->light.range=v.x; break;
      case 9: if(n->has_light)n->light.inner_cone=v.x; break;
      case 10: if(n->has_light)n->light.outer_cone=v.x; break;
      case 11: if(n->has_light)n->light.color={v.x,v.y,v.z}; break;
      default: break;
    }
  }
  return true;
}
}
