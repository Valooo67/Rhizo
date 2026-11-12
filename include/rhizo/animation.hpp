#pragma once
#include "rhizo/scene.hpp"
#include <map>
namespace rhizo {
enum class TrackProperty:uint32_t { Position=0,Rotation=1,Scale=2 };
struct TransformKey { double time=0; Transform value; };
struct AnimationTrack { EntityId entity=kInvalidEntity; TrackProperty property=TrackProperty::Position; std::map<double,TransformKey> keys; };
struct ParameterKey { double time=0; Vec4 value{}; };
struct ParameterTrack { EntityId entity=kInvalidEntity; uint32_t property=0; std::map<double,ParameterKey> keys; };
class Animation {
 public:
  bool set_key(EntityId entity,TrackProperty property,double time,const Transform& value);
  bool remove_key(EntityId entity,TrackProperty property,double time);
  bool set_parameter_key(EntityId entity,uint32_t property,double time,Vec4 value);
  bool evaluate(Scene& scene,double time)const;
  void set_duration(double seconds){duration_=seconds<0?0:seconds;}
  double duration()const{return duration_;}
  const std::vector<AnimationTrack>& tracks()const{return tracks_;}
 private:
  std::vector<AnimationTrack> tracks_;
  std::vector<ParameterTrack> parameter_tracks_;
  double duration_=0;
};
}
