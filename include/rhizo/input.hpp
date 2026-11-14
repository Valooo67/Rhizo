#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace rhizo {
struct InputTouch { bool active=false; float x=0,y=0,delta_x=0,delta_y=0; };
struct InputActionState { float value=0; bool pressed=false,just_pressed=false,just_released=false; };
class InputSystem {
 public:
  void begin_frame();
  bool touch_event(uint32_t,uint32_t,float,float);
  bool key_event(int32_t,bool);
  bool bind_key(const std::string&,int32_t);
  bool bind_touch_button(const std::string&,float,float,float,float);
  bool bind_touch_axis(const std::string&,float,float,float,float,uint32_t);
  bool get_action(const std::string&,InputActionState&)const;
  bool get_touch(uint32_t,InputTouch&)const;
  void remove_action(const std::string&);
  void clear();
 private:
  struct Zone { float x=0,y=0,w=0,h=0; uint32_t axis=0; bool button=true; };
  struct Action { std::vector<int32_t> keys; std::vector<Zone> zones; InputActionState state; };
  void recompute();
  std::unordered_map<std::string,Action> actions_;
  std::unordered_map<int32_t,bool> keys_;
  InputTouch touches_[10]{};
};
}
