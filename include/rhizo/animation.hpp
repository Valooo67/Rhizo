#pragma once
#include "rhizo/scene.hpp"
#include <map>
namespace rhizo {
enum class TrackProperty:uint32_t { Position=0,Rotation=1,Scale=2 };
struct TransformKey { double time=0; Transform value; };
struct AnimationTrack { EntityId entity=kInvalidEntity; TrackProperty property=TrackProperty::Position; std::map<double,TransformKey> keys; };
class Animation {
 public:
  bool set_key(EntityId entity,TrackProperty property,double time,const Transform& value);
  bool remove_key(EntityId entity,TrackProperty property,double time);
  bool evaluate(Scene& scene,double time)const;
  void set_duration(double seconds){duration_=seconds<0?0:seconds;}
  double duration()const{return duration_;}
  const std::vector<AnimationTrack>& tracks()const{return tracks_;}
 private:
  std::vector<AnimationTrack> tracks_;
  double duration_=0;
};
}
