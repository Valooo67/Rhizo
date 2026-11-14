#include "rhizo/input.hpp"
#include <algorithm>
#include <cmath>
namespace rhizo {
void InputSystem::begin_frame(){for(auto&[name,a]:actions_){(void)name;a.state.just_pressed=false;a.state.just_released=false;}for(auto&t:touches_){t.delta_x=0;t.delta_y=0;}}
bool InputSystem::touch_event(uint32_t id,uint32_t action,float x,float y){if(id>=10||action>3||!std::isfinite(x)||!std::isfinite(y))return false;auto&t=touches_[id];if(action==3){for(auto&p:touches_)p.active=false;}else if(action==0||action==1){const float oldX=t.x,oldY=t.y;t.active=true;t.delta_x=action==1?x-oldX:0;t.delta_y=action==1?y-oldY:0;t.x=std::clamp(x,0.0f,1.0f);t.y=std::clamp(y,0.0f,1.0f);}else{t.active=false;t.x=std::clamp(x,0.0f,1.0f);t.y=std::clamp(y,0.0f,1.0f);}recompute();return true;}
bool InputSystem::key_event(int32_t code,bool down){if(code<0||code>65535)return false;keys_[code]=down;recompute();return true;}
bool InputSystem::bind_key(const std::string&name,int32_t code){if(name.empty()||code<0||code>65535)return false;auto&a=actions_[name];if(std::find(a.keys.begin(),a.keys.end(),code)==a.keys.end())a.keys.push_back(code);recompute();return true;}
bool InputSystem::bind_touch_button(const std::string&name,float x,float y,float w,float h){if(name.empty()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||x<0||y<0||x+w>1||y+h>1)return false;actions_[name].zones.push_back({x,y,w,h,0,true});recompute();return true;}
bool InputSystem::bind_touch_axis(const std::string&name,float x,float y,float w,float h,uint32_t axis){if(name.empty()||axis>1||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||x<0||y<0||x+w>1||y+h>1)return false;actions_[name].zones.push_back({x,y,w,h,axis,false});recompute();return true;}
void InputSystem::recompute(){for(auto&[name,a]:actions_){(void)name;float value=0;for(int32_t k:a.keys){auto i=keys_.find(k);if(i!=keys_.end()&&i->second)value=1;}for(const auto&z:a.zones)for(const auto&t:touches_)if(t.active&&t.x>=z.x&&t.x<=z.x+z.w&&t.y>=z.y&&t.y<=z.y+z.h){if(z.button)value=1;else{const float q=z.axis==0?(t.x-z.x)/z.w:(t.y-z.y)/z.h;value=std::clamp((q-0.5f)*2.0f,-1.0f,1.0f);}}const bool down=std::fabs(value)>1e-4f;if(down&&!a.state.pressed)a.state.just_pressed=true;if(!down&&a.state.pressed)a.state.just_released=true;a.state.value=value;a.state.pressed=down;}}
bool InputSystem::get_action(const std::string&name,InputActionState&out)const{auto i=actions_.find(name);if(i==actions_.end())return false;out=i->second.state;return true;}
bool InputSystem::get_touch(uint32_t id,InputTouch&out)const{if(id>=10)return false;out=touches_[id];return true;}
void InputSystem::remove_action(const std::string&name){actions_.erase(name);}
void InputSystem::clear(){actions_.clear();keys_.clear();for(auto&t:touches_)t={};}
}
