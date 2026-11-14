#include "rhizo/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace rhizo {
namespace {
bool finite3(const float* p){return p&&std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);}
float clampf(float v,float a,float b){return std::clamp(v,a,b);}
uint16_t u16(const uint8_t*p){return static_cast<uint16_t>(p[0]|(uint16_t(p[1])<<8));}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
AudioEngine::~AudioEngine(){shutdown();}
bool AudioEngine::ensure_stream(std::string& error){
 if(stream_)return true;
 AAudioStreamBuilder* builder=nullptr;
 aaudio_result_t r=AAudio_createStreamBuilder(&builder);
 if(r!=AAUDIO_OK||!builder){error="AAudio stream builder creation failed: "+std::string(AAudio_convertResultToText(r));return false;}
 AAudioStreamBuilder_setDirection(builder,AAUDIO_DIRECTION_OUTPUT);
 AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_FLOAT);
 AAudioStreamBuilder_setChannelCount(builder,2);
 AAudioStreamBuilder_setSampleRate(builder,48000);
 AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
 AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
 AAudioStreamBuilder_setDataCallback(builder,&AudioEngine::callback,this);
 AAudioStreamBuilder_setErrorCallback(builder,&AudioEngine::error_callback,this);
 r=AAudioStreamBuilder_openStream(builder,&stream_);
 AAudioStreamBuilder_delete(builder);
 if(r!=AAUDIO_OK||!stream_){stream_=nullptr;error="AAudio output open failed: "+std::string(AAudio_convertResultToText(r));return false;}
 if(AAudioStream_getFormat(stream_)!=AAUDIO_FORMAT_PCM_FLOAT||AAudioStream_getChannelCount(stream_)!=2){AAudioStream_close(stream_);stream_=nullptr;error="AAudio returned an incompatible output format";return false;}
 r=AAudioStream_requestStart(stream_);
 if(r!=AAUDIO_OK){AAudioStream_close(stream_);stream_=nullptr;error="AAudio start failed: "+std::string(AAudio_convertResultToText(r));return false;}
 stream_failed_=false;return true;
}
bool AudioEngine::load_pcm(const float* data,uint64_t frames,uint32_t rate,uint32_t channels,uint32_t& id,std::string& error){
 id=0;if(!data||frames==0||frames>uint64_t(48000)*60u*30u||rate<8000||rate>192000||(channels!=1&&channels!=2)){error="Invalid PCM clip parameters";return false;}
 const uint64_t count=frames*channels;if(count>std::numeric_limits<size_t>::max()/sizeof(float)){error="PCM clip is too large";return false;}
 auto clip=std::make_shared<Clip>();clip->frames=frames;clip->sample_rate=rate;clip->channels=channels;
 try{clip->samples.resize(static_cast<size_t>(count));}catch(...){error="Unable to allocate PCM clip";return false;}
 for(uint64_t i=0;i<count;i++){if(!std::isfinite(data[i])){error="PCM samples must be finite";return false;}clip->samples[static_cast<size_t>(i)]=clampf(data[i],-1.0f,1.0f);}
 std::lock_guard lock(mutex_);id=next_clip_++;if(id==0)id=next_clip_++;clips_.emplace(id,std::move(clip));return true;
}
bool AudioEngine::load_wav(const char* path,uint32_t& id,std::string& error){
 id=0;if(!path||!*path){error="WAV path is empty";return false;}
 std::ifstream file(path,std::ios::binary);if(!file){error="Unable to open WAV file";return false;}
 file.seekg(0,std::ios::end);const auto length=file.tellg();if(length<44||length>std::streamoff(256u*1024u*1024u)){error="WAV file size is invalid";return false;}
 std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),length)){error="Unable to read WAV file";return false;}
 if(std::memcmp(bytes.data(),"RIFF",4)!=0||std::memcmp(bytes.data()+8,"WAVE",4)!=0){error="File is not a RIFF/WAVE stream";return false;}
 uint16_t format=0,channels=0,bits=0;uint32_t rate=0;size_t dataOffset=0,dataSize=0;
 for(size_t p=12;p+8<=bytes.size();){const uint32_t n=u32(bytes.data()+p+4);const size_t begin=p+8;if(n>bytes.size()-begin){error="WAV chunk exceeds file bounds";return false;}
  if(std::memcmp(bytes.data()+p,"fmt ",4)==0&&n>=16){format=u16(bytes.data()+begin);channels=u16(bytes.data()+begin+2);rate=u32(bytes.data()+begin+4);bits=u16(bytes.data()+begin+14);}
  if(std::memcmp(bytes.data()+p,"data",4)==0){dataOffset=begin;dataSize=n;}
  const size_t step=8u+static_cast<size_t>(n)+(n&1u);if(step>bytes.size()-p)break;p+=step;
 }
 if(dataSize==0||channels==0||channels>2||rate<8000||rate>192000||!((format==1&&(bits==16||bits==24||bits==32))||(format==3&&bits==32))){error="Unsupported WAV encoding; use mono/stereo PCM16/24/32 or IEEE float32";return false;}
 const uint32_t bytesPerSample=bits/8u;if(dataSize% (bytesPerSample*channels)!=0){error="WAV data chunk is not frame aligned";return false;}
 const uint64_t frames=dataSize/(bytesPerSample*channels);std::vector<float> pcm(static_cast<size_t>(frames*channels));
 for(uint64_t i=0;i<frames*channels;i++){const uint8_t* p=bytes.data()+dataOffset+static_cast<size_t>(i)*bytesPerSample;float v=0;
  if(format==3){std::memcpy(&v,p,4);}
  else if(bits==16){const int16_t q=static_cast<int16_t>(u16(p));v=static_cast<float>(q)/32768.0f;}
  else if(bits==24){int32_t q=int32_t(p[0])|(int32_t(p[1])<<8)|(int32_t(p[2])<<16);if(q&0x800000)q|=~0xffffff;v=static_cast<float>(q)/8388608.0f;}
  else {const int32_t q=static_cast<int32_t>(u32(p));v=static_cast<float>(static_cast<double>(q)/2147483648.0);}
  if(!std::isfinite(v)){error="WAV contains non-finite samples";return false;}pcm[static_cast<size_t>(i)]=clampf(v,-1.0f,1.0f);
 }
 return load_pcm(pcm.data(),frames,rate,channels,id,error);
}
bool AudioEngine::unload_clip(uint32_t id){std::lock_guard lock(mutex_);for(const auto&v:voices_)if(v.active&&v.clip==id)return false;return clips_.erase(id)>0;}
bool AudioEngine::play(uint32_t clip,bool spatial,bool loop,float gain,float pitch,const float* pos,uint32_t& source,std::string& error){
 source=0;if(!std::isfinite(gain)||gain<0||gain>4||!std::isfinite(pitch)||pitch<0.125f||pitch>4||(spatial&&!finite3(pos))){error="Invalid audio source parameters";return false;}
 std::lock_guard lock(mutex_);if(!clips_.contains(clip)){error="Audio clip ID does not exist";return false;}if(voices_.size()>=256){error="Audio voice limit (256) reached";return false;}
 if(!ensure_stream(error))return false;
 Voice v;v.id=next_voice_++;if(v.id==0)v.id=next_voice_++;v.clip=clip;v.spatial=spatial;v.loop=loop;v.gain=gain;v.pitch=pitch;if(pos)std::copy(pos,pos+3,v.position);voices_.push_back(v);source=v.id;return true;
}
bool AudioEngine::stop(uint32_t id){std::lock_guard lock(mutex_);auto i=std::find_if(voices_.begin(),voices_.end(),[&](const Voice&v){return v.id==id;});if(i==voices_.end())return false;i->active=false;return true;}
bool AudioEngine::pause(uint32_t id,bool value){std::lock_guard lock(mutex_);auto i=std::find_if(voices_.begin(),voices_.end(),[&](const Voice&v){return v.id==id;});if(i==voices_.end())return false;i->paused=value;return true;}
bool AudioEngine::set_source(uint32_t id,const float* p,float gain,float pitch){if(!finite3(p)||!std::isfinite(gain)||gain<0||gain>4||!std::isfinite(pitch)||pitch<0.125f||pitch>4)return false;std::lock_guard lock(mutex_);auto i=std::find_if(voices_.begin(),voices_.end(),[&](const Voice&v){return v.id==id&&v.active;});if(i==voices_.end())return false;std::copy(p,p+3,i->position);i->gain=gain;i->pitch=pitch;return true;}
bool AudioEngine::set_listener(const float* p,const float* f,const float* u){if(!finite3(p)||!finite3(f)||!finite3(u))return false;const float fl=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]),ul=std::sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);if(fl<1e-5f||ul<1e-5f)return false;std::lock_guard lock(mutex_);std::copy(p,p+3,listener_position_);for(int i=0;i<3;i++){listener_forward_[i]=f[i]/fl;listener_up_[i]=u[i]/ul;}return true;}
bool AudioEngine::set_master_volume(float v){if(!std::isfinite(v)||v<0||v>2)return false;std::lock_guard lock(mutex_);master_volume_=v;return true;}
aaudio_data_callback_result_t AudioEngine::callback(AAudioStream*,void* user,void* data,int32_t frames){return static_cast<AudioEngine*>(user)->mix(static_cast<float*>(data),frames);}
void AudioEngine::error_callback(AAudioStream*,void* user,aaudio_result_t){auto*self=static_cast<AudioEngine*>(user);std::lock_guard lock(self->mutex_);self->stream_failed_=true;}
aaudio_data_callback_result_t AudioEngine::mix(float* out,int32_t frames){
 if(!out||frames<=0)return AAUDIO_CALLBACK_RESULT_CONTINUE;std::fill(out,out+static_cast<size_t>(frames)*2u,0.0f);
 std::lock_guard lock(mutex_);
 for(auto&v:voices_){if(!v.active||v.paused)continue;auto ci=clips_.find(v.clip);if(ci==clips_.end()){v.active=false;continue;}const Clip&c=*ci->second;const double step=static_cast<double>(c.sample_rate)/48000.0*static_cast<double>(v.pitch);
  float attenuation=1.0f,pan=0.0f;if(v.spatial){const float dx=v.position[0]-listener_position_[0],dy=v.position[1]-listener_position_[1],dz=v.position[2]-listener_position_[2];const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);const float nx=distance>1e-5f?dx/distance:0,ny=distance>1e-5f?dy/distance:0,nz=distance>1e-5f?dz/distance:0;const float rightX=listener_forward_[1]*listener_up_[2]-listener_forward_[2]*listener_up_[1],rightY=listener_forward_[2]*listener_up_[0]-listener_forward_[0]*listener_up_[2],rightZ=listener_forward_[0]*listener_up_[1]-listener_forward_[1]*listener_up_[0];pan=clampf(nx*rightX+ny*rightY+nz*rightZ,-1,1);attenuation=1.0f/(1.0f+0.15f*distance+0.04f*distance*distance);}
  const float leftPan=std::sqrt(0.5f*(1.0f-pan)),rightPan=std::sqrt(0.5f*(1.0f+pan));const float gain=v.gain*master_volume_*attenuation;
  for(int32_t frame=0;frame<frames;frame++){if(v.cursor>=static_cast<double>(c.frames)){if(v.loop)v.cursor=std::fmod(v.cursor,static_cast<double>(c.frames));else{v.active=false;break;}}
   const uint64_t i0=static_cast<uint64_t>(v.cursor),i1=std::min(i0+1,c.frames-1);const float t=static_cast<float>(v.cursor-static_cast<double>(i0));float l0,r0,l1,r1;
   if(c.channels==1){l0=r0=c.samples[static_cast<size_t>(i0)];l1=r1=c.samples[static_cast<size_t>(i1)];}else{l0=c.samples[static_cast<size_t>(i0)*2u];r0=c.samples[static_cast<size_t>(i0)*2u+1u];l1=c.samples[static_cast<size_t>(i1)*2u];r1=c.samples[static_cast<size_t>(i1)*2u+1u];}
   const float l=l0+(l1-l0)*t,r=r0+(r1-r0)*t;out[static_cast<size_t>(frame)*2u]+=l*gain*leftPan*1.41421356f;out[static_cast<size_t>(frame)*2u+1u]+=r*gain*rightPan*1.41421356f;v.cursor+=step;
  }
 }
 voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice&v){return !v.active;}),voices_.end());
 for(size_t i=0;i<static_cast<size_t>(frames)*2u;i++)out[i]=clampf(out[i],-1.0f,1.0f);
 return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
void AudioEngine::shutdown(){std::lock_guard lock(mutex_);if(stream_){AAudioStream_requestStop(stream_);AAudioStream_close(stream_);stream_=nullptr;}voices_.clear();clips_.clear();stream_failed_=false;}
uint32_t AudioEngine::active_voice_count()const{std::lock_guard lock(mutex_);return static_cast<uint32_t>(std::count_if(voices_.begin(),voices_.end(),[](const Voice&v){return v.active;}));}
uint32_t AudioEngine::clip_count()const{std::lock_guard lock(mutex_);return static_cast<uint32_t>(clips_.size());}
float AudioEngine::master_volume()const{std::lock_guard lock(mutex_);return master_volume_;}
}
