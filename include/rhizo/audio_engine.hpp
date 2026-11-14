#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <aaudio/AAudio.h>

namespace rhizo {
class AudioEngine {
 public:
  AudioEngine()=default;
  ~AudioEngine();
  AudioEngine(const AudioEngine&)=delete;
  AudioEngine& operator=(const AudioEngine&)=delete;
  bool load_pcm(const float*,uint64_t,uint32_t,uint32_t,uint32_t&,std::string&);
  bool load_wav(const char*,uint32_t&,std::string&);
  bool unload_clip(uint32_t);
  bool play(uint32_t,bool,bool,float,float,const float*,uint32_t&,std::string&);
  bool stop(uint32_t);
  bool pause(uint32_t,bool);
  bool set_source(uint32_t,const float*,float,float);
  bool set_listener(const float*,const float*,const float*);
  bool set_master_volume(float);
  void shutdown();
  uint32_t active_voice_count()const;
  uint32_t clip_count()const;
  float master_volume()const;
 private:
  struct Clip { std::vector<float> samples; uint64_t frames=0; uint32_t sample_rate=48000,channels=2; };
  struct Voice { uint32_t id=0,clip=0; double cursor=0; float gain=1,pitch=1; float position[3]{}; bool spatial=false,loop=false,paused=false,active=true; };
  static aaudio_data_callback_result_t callback(AAudioStream*,void*,void*,int32_t);
  static void error_callback(AAudioStream*,void*,aaudio_result_t);
  aaudio_data_callback_result_t mix(float*,int32_t);
  bool ensure_stream(std::string&);
  mutable std::mutex mutex_;
  AAudioStream* stream_=nullptr;
  std::unordered_map<uint32_t,std::shared_ptr<Clip>> clips_;
  std::vector<Voice> voices_;
  float listener_position_[3]{},listener_forward_[3]{0,0,-1},listener_up_[3]{0,1,0};
  float master_volume_=1.0f;
  uint32_t next_clip_=1,next_voice_=1;
  bool stream_failed_=false;
};
}
