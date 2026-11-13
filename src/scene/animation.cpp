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
 if(id==kInvalidEntity||property<3||property>48||!std::isfinite(t)||t<0)return false;
 if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||!std::isfinite(value.w))return false;
 auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==property;});
 if(it==parameter_tracks_.end()){parameter_tracks_.push_back({id,property,Interpolation::Linear,{}});it=std::prev(parameter_tracks_.end());}
 it->keys[t]={t,value};duration_=std::max(duration_,t);return true;
}
bool Animation::remove_key(EntityId id,TrackProperty p,double t){auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==tracks_.end()||it->keys.erase(t)==0)return false;if(it->keys.empty())tracks_.erase(it);duration_=0;for(const auto&track:tracks_)if(!track.keys.empty())duration_=std::max(duration_,track.keys.rbegin()->first);for(const auto&track:parameter_tracks_)if(!track.keys.empty())duration_=std::max(duration_,track.keys.rbegin()->first);return true;}
bool Animation::remove_parameter_key(EntityId id,uint32_t p,double t){auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==parameter_tracks_.end()||it->keys.erase(t)==0)return false;if(it->keys.empty())parameter_tracks_.erase(it);duration_=0;for(const auto&track:tracks_)if(!track.keys.empty())duration_=std::max(duration_,track.keys.rbegin()->first);for(const auto&track:parameter_tracks_)if(!track.keys.empty())duration_=std::max(duration_,track.keys.rbegin()->first);return true;}
bool Animation::set_interpolation(EntityId id,TrackProperty p,Interpolation mode){if(static_cast<uint32_t>(mode)>3)return false;auto it=std::find_if(tracks_.begin(),tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==tracks_.end())return false;it->interpolation=mode;return true;}
bool Animation::set_parameter_interpolation(EntityId id,uint32_t p,Interpolation mode){if(static_cast<uint32_t>(mode)>3)return false;auto it=std::find_if(parameter_tracks_.begin(),parameter_tracks_.end(),[&](const auto&x){return x.entity==id&&x.property==p;});if(it==parameter_tracks_.end())return false;it->interpolation=mode;return true;}
size_t Animation::key_count()const{size_t n=0;for(const auto&t:tracks_)n+=t.keys.size();for(const auto&t:parameter_tracks_)n+=t.keys.size();return n;}
bool Animation::evaluate(Scene&scene,double time)const{
 if(!std::isfinite(time))return false;
 for(const auto&track:tracks_){auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;auto [lo,hi]=sample_pair(track.keys,time);const double span=hi->first-lo->first;float t=span>0?(track.interpolation==Interpolation::Step?(time>=hi->first?1.0f:0.0f):ease(track.interpolation,static_cast<float>((time-lo->first)/span))):0.0f;
  const Transform&a=lo->second.value,&b=hi->second.value;
  if(track.property==TrackProperty::Position){if(track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end())n->local.position=catmull(std::prev(lo)->second.value.position,a.position,b.position,std::next(hi)->second.value.position,t);else n->local.position=a.position+(b.position-a.position)*t;}
  else if(track.property==TrackProperty::Scale){if(track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end())n->local.scale=catmull(std::prev(lo)->second.value.scale,a.scale,b.scale,std::next(hi)->second.value.scale,t);else n->local.scale=a.scale+(b.scale-a.scale)*t;}
  else n->local.rotation=slerp(a.rotation,b.rotation,t);
 }
 for(const auto&track:parameter_tracks_){auto*n=scene.get(track.entity);if(!n||track.keys.empty())continue;auto [lo,hi]=sample_pair(track.keys,time);const double span=hi->first-lo->first;float t=span>0?(track.interpolation==Interpolation::Step?(time>=hi->first?1.0f:0.0f):ease(track.interpolation,static_cast<float>((time-lo->first)/span))):0.0f;const Vec4 a=lo->second.value,b=hi->second.value;Vec4 v=track.interpolation==Interpolation::Cubic&&lo!=track.keys.begin()&&std::next(hi)!=track.keys.end()?catmull4(std::prev(lo)->second.value,a,b,std::next(hi)->second.value,t):mix4(a,b,t);
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
   default:if(track.property>=17&&track.property<49){const size_t morph=track.property-17;if(n->morph_weights.size()<=morph)n->morph_weights.resize(morph+1,0.0f);n->morph_weights[morph]=std::clamp(v.x,-1.0f,1.0f);}break;
  }
 }
 return true;
}
void AnimationPlayer::seek(double seconds){if(std::isfinite(seconds))time_=std::clamp(seconds,0.0,animation_?animation_->duration():seconds);}
bool AnimationPlayer::evaluate(Scene&scene)const{return animation_&&animation_->evaluate(scene,time_);}
bool AnimationPlayer::tick(Scene&scene,double delta){if(!animation_||!std::isfinite(delta)||delta<0)return false;if(playing_){const double duration=animation_->duration();if(duration<=0){time_=0;playing_=false;}else{time_+=delta*speed_*static_cast<double>(direction_);if(mode_==PlaybackMode::Loop){time_=std::fmod(time_,duration);if(time_<0)time_+=duration;}else if(mode_==PlaybackMode::PingPong){while(time_<0||time_>duration){if(time_>duration){time_=duration-(time_-duration);direction_=-direction_;}else{time_=-time_;direction_=-direction_;}}}else if(time_>=duration||time_<=0){time_=std::clamp(time_,0.0,duration);playing_=false;}}}return evaluate(scene);}
bool AnimationStateMachine::add_state(std::string name,const Animation*animation,double speed){if(name.empty()||!animation||!std::isfinite(speed)||speed==0)return false;states_[name]={name,animation,speed};if(current_.empty())play(name);return true;}
bool AnimationStateMachine::set_transition(const std::string&from,const std::string&to,double duration){if(!states_.contains(from)||!states_.contains(to)||!std::isfinite(duration)||duration<0)return false;transitions_[from+"\n"+to]=duration;return true;}
bool AnimationStateMachine::play(const std::string&state,bool restart){
 auto it=states_.find(state);if(it==states_.end())return false;if(state==current_&&!restart)return true;
 double fade=0.0;if(!current_.empty()){auto tr=transitions_.find(current_+"\n"+state);if(tr!=transitions_.end())fade=tr->second;}
 if(fade>0.0&&player_.playing()){outgoing_player_=player_;blending_=true;transition_={current_,state,fade,0.0};}else{blending_=false;transition_={};}
 current_=state;player_.bind(it->second.animation);player_.set_speed(it->second.speed);player_.play(restart);return true;
}
bool AnimationStateMachine::update(Scene&scene,double delta){
 if(!std::isfinite(delta)||delta<0)return false;
 if(!blending_)return player_.tick(scene,delta);
 Scene from=scene,to=scene;outgoing_player_.tick(from,delta);player_.tick(to,delta);
 transition_.elapsed=std::min(transition_.duration,transition_.elapsed+delta);const float w=static_cast<float>(transition_progress());
 auto&dst=scene.nodes();const auto&aa=from.nodes();const auto&bb=to.nodes();
 for(size_t i=0;i<dst.size()&&i<aa.size()&&i<bb.size();++i){
  dst[i].local.position=aa[i].local.position+(bb[i].local.position-aa[i].local.position)*w;
  dst[i].local.scale=aa[i].local.scale+(bb[i].local.scale-aa[i].local.scale)*w;
  dst[i].local.rotation=slerp(aa[i].local.rotation,bb[i].local.rotation,w);
  const auto&ca=aa[i].camera;const auto&cb=bb[i].camera;auto&cd=dst[i].camera;
  if(dst[i].has_camera&&aa[i].has_camera&&bb[i].has_camera){cd.vertical_fov=ca.vertical_fov+(cb.vertical_fov-ca.vertical_fov)*w;cd.near_plane=ca.near_plane+(cb.near_plane-ca.near_plane)*w;cd.far_plane=ca.far_plane+(cb.far_plane-ca.far_plane)*w;cd.orthographic_size=ca.orthographic_size+(cb.orthographic_size-ca.orthographic_size)*w;}
  const auto&la=aa[i].light;const auto&lb=bb[i].light;auto&ld=dst[i].light;
  if(dst[i].has_light&&aa[i].has_light&&bb[i].has_light){ld.intensity=la.intensity+(lb.intensity-la.intensity)*w;ld.range=la.range+(lb.range-la.range)*w;ld.inner_cone=la.inner_cone+(lb.inner_cone-la.inner_cone)*w;ld.outer_cone=la.outer_cone+(lb.outer_cone-la.outer_cone)*w;ld.color=la.color+(lb.color-la.color)*w;}
  const size_t morphCount=std::min(aa[i].morph_weights.size(),bb[i].morph_weights.size());dst[i].morph_weights.resize(std::max(aa[i].morph_weights.size(),bb[i].morph_weights.size()));for(size_t j=0;j<morphCount;j++)dst[i].morph_weights[j]=aa[i].morph_weights[j]+(bb[i].morph_weights[j]-aa[i].morph_weights[j])*w;
 }
 for(size_t i=0;i<scene.materials.size()&&i<from.materials.size()&&i<to.materials.size();++i){auto&m=scene.materials[i];const auto&a=from.materials[i];const auto&b=to.materials[i];m.base_color=mix4(a.base_color,b.base_color,w);m.metallic=a.metallic+(b.metallic-a.metallic)*w;m.roughness=a.roughness+(b.roughness-a.roughness)*w;m.normal_scale=a.normal_scale+(b.normal_scale-a.normal_scale)*w;m.occlusion=a.occlusion+(b.occlusion-a.occlusion)*w;m.emissive_color=a.emissive_color+(b.emissive_color-a.emissive_color)*w;}
 if(w>=1.0f)blending_=false;return true;
}
double AnimationStateMachine::transition_progress()const{return transition_.duration>0?std::clamp(transition_.elapsed/transition_.duration,0.0,1.0):1.0;}

namespace {
Quat multiply_quat(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Quat from_to(Vec3 a,Vec3 b){a=normalize(a);b=normalize(b);const float d=std::clamp(dot(a,b),-1.0f,1.0f);if(d>0.99999f)return Quat::identity();if(d<-0.99999f){Vec3 axis=normalize(cross(a,std::fabs(a.x)<0.8f?Vec3{1,0,0}:Vec3{0,1,0}));return {axis.x,axis.y,axis.z,0};}const Vec3 c=cross(a,b);const float s=std::sqrt((1.0f+d)*2.0f),inv=1.0f/s;return {c.x*inv,c.y*inv,c.z*inv,s*0.5f};}
Vec3 matrix_position(const Mat4&m){return {m.v[12],m.v[13],m.v[14]};}
Quat conjugate(Quat q){return {-q.x,-q.y,-q.z,q.w};}
Quat quat_from_matrix(const Mat4&m);\nQuat normalized_quat(Quat q){const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return n>1.0e-8f?Quat{q.x/n,q.y/n,q.z/n,q.w/n}:Quat::identity();}
Quat world_rotation(const Scene&scene,EntityId id){return normalized_quat(quat_from_matrix(scene.world_matrix(id)));}
Quat quat_from_matrix(const Mat4&m){
 Vec3 x=normalize({m.v[0],m.v[1],m.v[2]}),y=normalize({m.v[4],m.v[5],m.v[6]}),z=normalize({m.v[8],m.v[9],m.v[10]});
 const float m00=x.x,m01=y.x,m02=z.x,m10=x.y,m11=y.y,m12=z.y,m20=x.z,m21=y.z,m22=z.z,trace=m00+m11+m22;Quat q;
 if(trace>0){const float t=std::sqrt(trace+1.0f)*2.0f;q.w=0.25f*t;q.x=(m21-m12)/t;q.y=(m02-m20)/t;q.z=(m10-m01)/t;}
 else if(m00>m11&&m00>m22){const float t=std::sqrt(1.0f+m00-m11-m22)*2.0f;q.w=(m21-m12)/t;q.x=0.25f*t;q.y=(m01+m10)/t;q.z=(m02+m20)/t;}
 else if(m11>m22){const float t=std::sqrt(1.0f+m11-m00-m22)*2.0f;q.w=(m02-m20)/t;q.x=(m01+m10)/t;q.y=0.25f*t;q.z=(m12+m21)/t;}
 else{const float t=std::sqrt(1.0f+m22-m00-m11)*2.0f;q.w=(m10-m01)/t;q.x=(m02+m20)/t;q.y=(m12+m21)/t;q.z=0.25f*t;}
 return normalized_quat(q);
}
void set_world_rotation(Scene&scene,SceneNode&node,Quat desired,float weight){const Quat current=world_rotation(scene,node.id);const Quat blended=slerp(current,desired,std::clamp(weight,0.0f,1.0f));const Quat parent=node.parent?world_rotation(scene,node.parent):Quat::identity();node.local.rotation=normalized_quat(multiply_quat(conjugate(parent),blended));}
}

bool retarget_animation(const Animation&source,const std::unordered_map<EntityId,EntityId>&mapping,Animation&destination){
 Animation result;result.set_duration(source.duration());
 for(const auto&track:source.tracks()){auto it=mapping.find(track.entity);if(it==mapping.end())continue;for(const auto&[time,key]:track.keys)if(!result.set_key(it->second,track.property,time,key.value))return false;if(!track.keys.empty())result.set_interpolation(it->second,track.property,track.interpolation);}
 for(const auto&track:source.parameter_tracks()){auto it=mapping.find(track.entity);if(it==mapping.end())continue;for(const auto&[time,key]:track.keys)if(!result.set_parameter_key(it->second,track.property,time,key.value))return false;if(!track.keys.empty())result.set_parameter_interpolation(it->second,track.property,track.interpolation);}
 destination=std::move(result);return true;
}
bool solve_look_at(Scene&scene,EntityId entity,EntityId target,float weight){
 auto*n=scene.get(entity);auto*t=scene.get(target);if(!n||!t||entity==target||!std::isfinite(weight))return false;weight=std::clamp(weight,0.0f,1.0f);const Vec3 origin=matrix_position(scene.world_matrix(entity)),destination=matrix_position(scene.world_matrix(target));const Vec3 direction=destination-origin;if(length(direction)<1.0e-5f)return false;const Mat4 world=scene.world_matrix(entity);const Vec3 forward=normalize({-world.v[8],-world.v[9],-world.v[10]});const Quat delta=from_to(forward,direction);set_world_rotation(scene,*n,multiply_quat(delta,world_rotation(scene,entity)),weight);return true;
}
bool solve_two_bone_ik(Scene&scene,EntityId root,EntityId middle,EntityId end,Vec3 target,Vec3 pole,float weight){
 auto*r=scene.get(root);auto*m=scene.get(middle);auto*e=scene.get(end);if(!r||!m||!e||!std::isfinite(weight)||root==middle||middle==end||m->parent!=root||e->parent!=middle)return false;weight=std::clamp(weight,0.0f,1.0f);if(weight<=0)return true;
 Vec3 rp=matrix_position(scene.world_matrix(root)),mp=matrix_position(scene.world_matrix(middle)),ep=matrix_position(scene.world_matrix(end));const float l1=length(mp-rp),l2=length(ep-mp);Vec3 to=target-rp;float d=length(to);if(l1<1e-5f||l2<1e-5f||d<1e-5f)return false;const Vec3 dir=to/d;d=std::clamp(d,std::fabs(l1-l2)+1e-4f,l1+l2-1e-4f);Vec3 bend=pole-rp-dir*dot(pole-rp,dir);if(length(bend)<1e-5f)bend=cross(dir,{0,1,0});if(length(bend)<1e-5f)bend=cross(dir,{1,0,0});bend=normalize(bend);const float along=(d*d+l1*l1-l2*l2)/(2.0f*d),height=std::sqrt(std::max(0.0f,l1*l1-along*along));const Vec3 desired=rp+dir*along+bend*height;
 const Quat rootDelta=from_to(mp-rp,desired-rp);set_world_rotation(scene,*r,multiply_quat(rootDelta,world_rotation(scene,root)),weight);
 mp=matrix_position(scene.world_matrix(middle));ep=matrix_position(scene.world_matrix(end));const Quat midDelta=from_to(ep-mp,target-mp);set_world_rotation(scene,*m,multiply_quat(midDelta,world_rotation(scene,middle)),weight);return true;
}
} // namespace rhizo
