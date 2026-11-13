#include "rhizo/animation.hpp"
#include <cassert>
#include <cmath>
#include <unordered_map>

using namespace rhizo;

static bool near(float a,float b,float eps=1.0e-3f){return std::fabs(a-b)<eps;}

int main(){
 Scene scene;const EntityId a=scene.create("A"),b=scene.create("B");assert(a&&b);
 Animation step;Transform k0,k1;k0.position={0,0,0};k1.position={10,0,0};
 assert(step.set_key(a,TrackProperty::Position,0,k0));assert(step.set_key(a,TrackProperty::Position,1,k1));assert(step.set_interpolation(a,TrackProperty::Position,Interpolation::Step));
 assert(step.evaluate(scene,0.5));assert(near(scene.get(a)->local.position.x,0));assert(step.evaluate(scene,1.0));assert(near(scene.get(a)->local.position.x,10));
 assert(step.remove_key(a,TrackProperty::Position,1));assert(near(static_cast<float>(step.duration()),0));

 Animation morph;assert(morph.set_parameter_key(a,17,0,{0,0,0,0}));assert(morph.set_parameter_key(a,17,1,{1,0,0,0}));assert(morph.evaluate(scene,0.5));assert(scene.get(a)->morph_weights.size()==1);assert(near(scene.get(a)->morph_weights[0],0.5f));

 Animation loop;assert(loop.set_key(a,TrackProperty::Position,0,k0));assert(loop.set_key(a,TrackProperty::Position,1,k1));
 AnimationPlayer player;player.bind(&loop);player.set_mode(PlaybackMode::Loop);player.play(true);assert(player.tick(scene,1.25));assert(near(static_cast<float>(player.time()),0.25f));
 player.bind(&loop);player.set_mode(PlaybackMode::PingPong);player.play(true);assert(player.tick(scene,1.25));assert(near(static_cast<float>(player.time()),0.75f));

 Animation source;assert(source.set_key(a,TrackProperty::Position,0,k0));assert(source.set_key(a,TrackProperty::Position,1,k1));Animation target;std::unordered_map<EntityId,EntityId> mapping{{a,b}};assert(retarget_animation(source,mapping,target));assert(target.evaluate(scene,1));assert(near(scene.get(b)->local.position.x,10));

 Animation first,second;Transform f0,f1,s0,s1;f0.position={0,0,0};f1.position={10,0,0};s0.position={0,0,0};s1.position={20,0,0};
 assert(first.set_key(a,TrackProperty::Position,0,f0));assert(first.set_key(a,TrackProperty::Position,1,f1));assert(second.set_key(a,TrackProperty::Position,0,s0));assert(second.set_key(a,TrackProperty::Position,1,s1));
 AnimationStateMachine machine;assert(machine.add_state("idle",&first));assert(machine.add_state("run",&second));assert(machine.set_transition("idle","run",1.0));assert(machine.play("idle",true));assert(machine.update(scene,0.5));assert(machine.play("run",true));assert(machine.update(scene,0.5));assert(near(scene.get(a)->local.position.x,10));
 return 0;
}
