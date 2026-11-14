#include "rhizo/input.hpp"
#include "rhizo/particle_system.hpp"
#include "rhizo/rhizo.h"
#include <cassert>
#include <cmath>
int main(){
 using namespace rhizo;
 InputSystem input;
 assert(input.bind_key("jump",32));assert(input.key_event(32,true));
 InputActionState state{};
 assert(input.get_action("jump",state)&&state.pressed&&state.just_pressed);
 input.begin_frame();assert(input.get_action("jump",state)&&state.pressed&&!state.just_pressed);
 assert(input.key_event(32,false));assert(input.get_action("jump",state)&&!state.pressed&&state.just_released);
 assert(input.bind_touch_button("fire",0.4f,0.4f,0.2f,0.2f));
 assert(input.touch_event(0,RHIZO_TOUCH_DOWN,0.5f,0.5f));assert(input.get_action("fire",state)&&state.pressed);
 assert(input.touch_event(0,RHIZO_TOUCH_UP,0.5f,0.5f));assert(input.get_action("fire",state)&&!state.pressed&&state.just_released);
 assert(input.bind_touch_axis("move",0.2f,0.2f,0.6f,0.6f,0));
 assert(input.touch_event(1,RHIZO_TOUCH_DOWN,0.8f,0.5f));assert(input.get_action("move",state)&&state.value>0.99f);
 Scene scene;const EntityId emitter=scene.create("Emitter");ParticleSystem particles;
 RhizoParticleEmitterInfo info{};info.struct_size=sizeof(info);info.version=1;info.capacity=16;info.shape=RHIZO_PARTICLE_POINT;
 info.lifetime_min=0.5f;info.lifetime_max=0.5f;info.speed_min=2;info.speed_max=2;info.size_start=1;info.size_end=0.25f;
 info.color_start[0]=1;info.color_start[1]=0.5f;info.color_start[2]=0.25f;info.color_start[3]=1;
 info.color_end[0]=0;info.color_end[1]=0;info.color_end[2]=0;info.color_end[3]=0;
 info.direction[1]=1;info.seed=1234;info.enabled=1;
 assert(particles.set_emitter(scene,emitter,info));
 assert(scene.components.get<CustomComponentSet>(emitter)->values.contains(0x5052544cu));
 assert(particles.burst(scene,emitter,3));assert(particles.count(emitter)==3);
 assert(particles.step(scene,0.25,nullptr,nullptr));assert(particles.count(emitter)==3);
 RhizoParticle out[16]{};assert(particles.copy(emitter,out,16)==3);
 assert(std::fabs(out[0].position[1]-0.5f)<0.001f);assert(out[0].size<1.0f&&out[0].color[0]<1.0f);
 assert(particles.step(scene,0.3,nullptr,nullptr));assert(particles.count(emitter)==0);
 assert(particles.remove_emitter(scene,emitter));return 0;
}
