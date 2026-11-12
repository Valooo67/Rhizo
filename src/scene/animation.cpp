#include "rhizo/animation.hpp"
#include <algorithm>
namespace rhizo {
bool Animation::set_key(EntityId id,TrackProperty p,double t,const Transform&v){
  if(id==kInvalidEntity||!std::isfinite(t)||t<0)return false;
  auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});
  if(it==tracks_.end()){tracks_.push_back({id,p,{}});it=std::prev(tracks_.end());}
  it->keys[t]={t,v};duration_=std::max(duration_,t);return true;
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
  return true;
}
}
