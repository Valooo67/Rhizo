#include "rhizo/animation.hpp"
#include <algorithm>
#include <cmath>

namespace rhizo {
namespace {
float ease(Interpolation mode,float t){t=std::clamp(t,0.0f,1.0f);if(mode==Interpolation::Step)return 0.0f;if(mode==Interpolation::Smooth)return t*t*(3.0f-2.0f*t);return t;}
Vec3 catmull(Vec3 p0,Vec3 p1,Vec3 p2,Vec3 p3,float t){const float t2=t*t,t3=t2*t;return (p1*2.0f+(p2-p0)*t+(p0*2.0f-p1*5.0f+p2*4.0f-p3)*t2+((p1-p2)*3.0f+p3-p0)*t3)*0.5f;}
Vec4 mix4(Vec4 a,Vec4 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};}
Vec4 catmull4(Vec4 p0,Vec4 p1,Vec4 p2,Vec4 p3,float t){auto c=[&](float a,float b,float c,float d){return 0.5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t);};return {c(p0.x,p1.x,p2.x,p3.x),c(p0.y,p1.y,p2.y,p3.y),c(p0.z,p1.z,p2.z,p3.z),c(p0.w,p1.w,p2.w,p3.w)};}
template<class Map> auto sample_pair(const Map& keys,double time){auto hi=keys.lower_bound(time);if(hi==keys.begin())return std::pair{hi,hi};if(hi==keys.end()){auto last=std::prev(hi);return std::pair{last,last};}return std::pair{std::prev(hi),hi};}
}
bool Animation::set_key(EntityId id,TrackProperty p,double t,const Transform&v){
 if(id==kInvalidEntity||!std::isfinite(t)||t<0)return false;
 const float values[]={v.position.x,v.position.y,v.position.z,v.rotation.x,v.rotation.y,v.rotation.z,v.rotation.w,v.scale.x,v.scale.y,v.scale.z};for(float x:values)if(!std::isfinite(x))return false;
 auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});
 if(it==tracks_.end()){tracks_.push_back({id,p,Interpolation::Linear,{}});it=std::prev(tracks_.end());}
 it->keys[t]={t,v};duration_=std::max(duration_,t);return true;
}
bool Animation::set_parameter_key(EntityId id,uint32_t property,double t,Vec4 value){
 if(id==kInvalidEntity||property<3||property>16||!std::isfinite(t)||t<0)return false;
 if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||!std::isfinite(value.w))return false;
 auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==property;});
 if(it==parameter_tracks_.end()){parameter_tracks_.push_back({id,property,Interpolation::Linear,{}});it=std::prev(parameter_tracks_.end());}
 it->keys[t]={t,value};duration_=std::max(duration_,t);return true;
}
bool Animation::remove_key(EntityId id,TrackProperty p,double t){auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});return it!=tracks_.end()&&it->keys.erase(t)>0;}
bool Animation::remove_parameter_key(EntityId id,uint32_t p,double t){auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});return it!=parameter_tracks_.end()&&it->keys.erase(t)>0;}
bool Animation::set_interpolation(EntityId id,TrackProperty p,Interpolation mode){if(static_cast<uint32_t>(mode)>3)return false;auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==tracks_.end())return false;it->interpolation=mode;return true;}
bool Animation::set_parameter_interpolation(EntityId id,uint32_t p,Interpolation mode){if(static_cast<uint32_t>(mode)>3)return false;auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==parameter_tracks_.end())return false;it->interpolation=mode;return true;}
size_t Animation::key_count()const{size_t n=0;for(const auto&t:tracks_)n+=t.keys.size();for(const auto&t:parameter_tracks_)n+=t.keys.size();return n;}
bool Animation::evaluate(Scene&scene,double time)const{
 if(!std::isfinite(time))return false;
 for(const auto&track:tracks_){auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;auto [lo,hi]=sample_pair(track.keys,time);const double span=hi->first-lo->first;float t=span>0?ease(track.interpolation,static_cast<float>((time-lo->first)/span)):0.0f;
  const Transform&a=lo->second.value,&b=hi->second.value;
  if(track.property==TrackProperty::Position){if(track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end())n->local.position=catmull(std::prev(lo)->second.value.position,a.position,b.position,std::next(hi)->second.value.position,t);else n->local.position=a.position+(b.position-a.position)*t;}
  else if(track.property==TrackProperty::Scale){if(track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end())n->local.scale=catmull(std::prev(lo)->second.value.scale,a.scale,b.scale,std::next(hi)->second.value.scale,t);else n->local.scale=a.scale+(b.scale-a.scale)*t;}
  else n->local.rotation=slerp(a.rotation,b.rotation,t);
 }
 for(const auto&track:parameter_tracks_){auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;auto [lo,hi]=sample_pair(track.keys,time);const double span=hi->first-lo->first;float t=span>0?ease(track.interpolation,static_cast<float>((time-lo->first)/span)):0.0f;const Vec4 a=lo->second.value,b=hi->second.value;Vec4 v=track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end()?catmull4(std::prev(lo)->second.value,a,b,std::next(hi)->second.value,t):mix4(a,b,t);
  switch(track.property){
   case 3:if(n->has_camera)n->camera.vertical_fov=std::clamp(v.x,0.01f,3.13f);break;
   case 4:if(n->has_camera)n->camera.near_plane=std::max(v.x,0.001f);break;
   case 5:if(n->has_camera)n->camera.far_plane=std::max(v.x,n->camera.near_plane+0.001f);break;
   case 6:if(n->has_camera)n->camera.orthographic_size=std::max(v.x,0.001f);break;
   case 7:if(n->has_light)n->light.intensity=std::max(v.x,0.0f);break;
   case 8:if(n->has_light)n->light.range=std::max(v.x,0.001f);break;
   case 9:if(n->has_light)n->light.inner_cone=std::clamp(v.x,0.0f,1.56f);break;
   case 10:if(n->has_light)n->light.outer_cone=std::clamp(v.x,n->light.inner_cone+0.001f,1.5707f);break;
   case 11:if(n->has_light)n->light.color={v.x,v.y,v.z};break;
   case 12:for(uint32_t i:n->mesh_indices)if(i<scene.meshes.size()&&scene.meshes[i].material_index<scene.materials.size())scene.materials[scene.meshes[i].material_index].base_color=v;break;
   case 13:for(uint32_t i:n->mesh_indices)if(i<scene.meshes.size()&&scene.meshes[i].material_index<scene.materials.size())scene.materials[scene.meshes[i].material_index].metallic=std::clamp(v.x,0.0f,1.0f);break;
   case 14:for(uint32_t i:n->mesh_indices)if(i<scene.meshes.size()&&scene.meshes[i].material_index<scene.materials.size())scene.materials[scene.meshes[i].material_index].roughness=std::clamp(v.x,0.045f,1.0f);break;
   case 15:for(uint32_t i:n->mesh_indices)if(i<scene.meshes.size()&&scene.meshes[i].material_index<scene.materials.size())scene.materials[scene.meshes[i].material_index].normal_scale=v.x;break;
   case 16:for(uint32_t i:n->mesh_indices)if(i<scene.meshes.size()&&scene.meshes[i].material_index<scene.materials.size())scene.materials[scene.meshes[i].material_index].occlusion=std::clamp(v.x,0.0f,1.0f);break;
   default:break;
  }
 }
 return true;
}
void AnimationPlayer::seek(double seconds){if(std::isfinite(seconds))time_=std::clamp(seconds,0.0,animation_?animation_->duration():seconds);}
bool AnimationPlayer::evaluate(Scene&scene)const{return animation_&&animation_->evaluate(scene,time_);}
bool AnimationPlayer::tick(Scene&scene,double delta){if(!animation_||!std::isfinite(delta)||delta<0)return false;if(playing_){const double duration=animation_->duration();if(duration<=0){time_=0;playing_=false;}else{time_+=delta*speed_*static_cast<double>(direction_);if(mode_==PlaybackMode::Loop){time_=std::fmod(time_,duration);if(time_<0)time_+=duration;}else if(mode_==PlaybackMode::PingPong){while(time_<0||time_>duration){if(time_>duration){time_=duration-(time_-duration);direction_=-direction_;}else{time_=-time_;direction_=-direction_;}}}else if(time_>=duration||time_<=0){time_=std::clamp(time_,0.0,duration);playing_=false;}}}return evaluate(scene);}
bool AnimationStateMachine::add_state(std::string name,const Animation*animation,double speed){if(name.empty()||!animation||!std::isfinite(speed)||speed==0)return false;states_[name]={name,animation,speed};if(current_.empty())play(name);return true;}
bool AnimationStateMachine::set_transition(const std::string&from,const std::string&to,double duration){if(!states_.contains(from)||!states_.contains(to)||!std::isfinite(duration)||duration<0)return false;transitions_[from+"\n"+to]=duration;return true;}
bool AnimationStateMachine::play(const std::string&state,bool restart){auto it=states_.find(state);if(it==states_.end())return false;current_=state;player_.bind(it->second.animation);player_.set_speed(it->second.speed);player_.play(restart);transition_={};return true;}
bool AnimationStateMachine::update(Scene&scene,double delta){if(!std::isfinite(delta)||delta<0)return false;return player_.tick(scene,delta);}
double AnimationStateMachine::transition_progress()const{return transition_.duration>0?std::clamp(transition_.elapsed/transition_.duration,0.0,1.0):1.0;}
} // namespace rhizo
