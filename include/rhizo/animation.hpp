#pragma once
#include "rhizo/scene.hpp"
#include <map>
#include <string>
#include <unordered_map>

namespace rhizo {
enum class TrackProperty:uint32_t { Position=0,Rotation=1,Scale=2 };
enum class Interpolation:uint32_t { Step=0,Linear=1,Smooth=2,Cubic=3 };
struct TransformKey { double time=0; Transform value; };
struct AnimationTrack { EntityId entity=kInvalidEntity; TrackProperty property=TrackProperty::Position; Interpolation interpolation=Interpolation::Linear; std::map<double,TransformKey> keys; };
struct ParameterKey { double time=0; Vec4 value{}; };
struct ParameterTrack { EntityId entity=kInvalidEntity; uint32_t property=0; Interpolation interpolation=Interpolation::Linear; std::map<double,ParameterKey> keys; };

class Animation {
 public:
  bool set_key(EntityId entity,TrackProperty property,double time,const Transform& value);
  bool remove_key(EntityId entity,TrackProperty property,double time);
  bool set_parameter_key(EntityId entity,uint32_t property,double time,Vec4 value);
  bool remove_parameter_key(EntityId entity,uint32_t property,double time);
  bool set_interpolation(EntityId entity,TrackProperty property,Interpolation mode);
  bool set_parameter_interpolation(EntityId entity,uint32_t property,Interpolation mode);
  bool evaluate(Scene& scene,double time)const;
  void set_duration(double seconds){if(std::isfinite(seconds)&&seconds>=0)duration_=seconds;}
  double duration()const{return duration_;}
  size_t key_count()const;
  const std::vector<AnimationTrack>& tracks()const{return tracks_;}
  const std::vector<ParameterTrack>& parameter_tracks()const{return parameter_tracks_;}
 private:
  std::vector<AnimationTrack> tracks_;
  std::vector<ParameterTrack> parameter_tracks_;
  double duration_=0;
};

enum class PlaybackMode:uint32_t { Once=0,Loop=1,PingPong=2 };
class AnimationPlayer {
 public:
  void bind(const Animation* animation){animation_=animation;time_=0;direction_=1;playing_=false;}
  void play(bool restart=false){if(restart)time_=0;direction_=1;playing_=animation_!=nullptr;}
  void pause(){playing_=false;}
  void stop(){playing_=false;time_=0;direction_=1;}
  void seek(double seconds);
  void set_speed(double speed){if(std::isfinite(speed))speed_=std::clamp(speed,-8.0,8.0);}
  void set_mode(PlaybackMode mode){mode_=mode;}
  bool tick(Scene& scene,double delta_seconds);
  bool evaluate(Scene& scene)const;
  double time()const{return time_;}
  bool playing()const{return playing_;}
 private:
  const Animation* animation_=nullptr;
  double time_=0,speed_=1;
  int direction_=1;
  PlaybackMode mode_=PlaybackMode::Once;
  bool playing_=false;
};

struct AnimationState { std::string name; const Animation* animation=nullptr; double speed=1.0; };
class AnimationStateMachine {
 public:
  bool add_state(std::string name,const Animation* animation,double speed=1.0);
  bool set_transition(const std::string& from,const std::string& to,double duration);
  bool play(const std::string& state,bool restart=true);
  bool update(Scene& scene,double delta_seconds);
  const std::string& current_state()const{return current_;}
  double transition_progress()const;
 private:
  struct Transition { std::string from,to; double duration=0,elapsed=0; };
  std::unordered_map<std::string,AnimationState> states_;
  std::unordered_map<std::string,double> transitions_;
  std::string current_;
  AnimationPlayer player_;
  Transition transition_;
  double previous_time_=0;
};

} // namespace rhizo
