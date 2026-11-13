#pragma once
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cstddef>

namespace rhizo {
struct Vec2 { float x=0, y=0; };
struct Vec3 {
  float x=0, y=0, z=0;
  Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
  Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
  Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
  Vec3 operator/(float s) const { return {x/s,y/s,z/s}; }
};
inline float dot(Vec3 a, Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a, Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(Vec3 v){return std::sqrt(dot(v,v));}
inline Vec3 normalize(Vec3 v){const float l=length(v);return l>1e-8f?v/l:Vec3{0,0,1};}
struct Vec4 { float x=0,y=0,z=0,w=0; };
struct Quat {
  float x=0,y=0,z=0,w=1;
  static Quat identity(){return {};}
  static Quat from_euler(Vec3 r) {
    const float cx=std::cos(r.x*.5f), sx=std::sin(r.x*.5f);
    const float cy=std::cos(r.y*.5f), sy=std::sin(r.y*.5f);
    const float cz=std::cos(r.z*.5f), sz=std::sin(r.z*.5f);
    return {sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz};
  }
};
struct Mat4 {
  std::array<float,16> v{};
  static Mat4 identity(){Mat4 m;m.v={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};return m;}
  const float* data() const{return v.data();}
};
inline Mat4 multiply(const Mat4&a,const Mat4&b){
  Mat4 o{};
  for(size_t c=0;c<4u;c++)for(size_t r=0;r<4u;r++)for(size_t k=0;k<4u;k++)o.v[c*4u+r]+=a.v[k*4u+r]*b.v[c*4u+k];
  return o;
}
inline Mat4 inverse(const Mat4&input){
 float a[4][8]{};for(size_t r=0;r<4;r++){for(size_t c=0;c<4;c++)a[r][c]=input.v[c*4+r];a[r][r+4]=1.0f;}
 for(size_t c=0;c<4;c++){size_t pivot=c;for(size_t r=c+1;r<4;r++)if(std::fabs(a[r][c])>std::fabs(a[pivot][c]))pivot=r;if(std::fabs(a[pivot][c])<1.0e-10f)return Mat4::identity();if(pivot!=c)for(size_t k=0;k<8;k++)std::swap(a[pivot][k],a[c][k]);const float divisor=a[c][c];for(size_t k=0;k<8;k++)a[c][k]/=divisor;for(size_t r=0;r<4;r++)if(r!=c){const float factor=a[r][c];for(size_t k=0;k<8;k++)a[r][k]-=factor*a[c][k];}}
 Mat4 out{};for(size_t r=0;r<4;r++)for(size_t c=0;c<4;c++)out.v[c*4+r]=a[r][c+4];return out;
}
inline Mat4 compose(Vec3 p,Quat q,Vec3 s){
  const float x=q.x,y=q.y,z=q.z,w=q.w;
  Mat4 m=Mat4::identity();
  m.v[0]=(1-2*(y*y+z*z))*s.x; m.v[1]=(2*(x*y+z*w))*s.x; m.v[2]=(2*(x*z-y*w))*s.x;
  m.v[4]=(2*(x*y-z*w))*s.y; m.v[5]=(1-2*(x*x+z*z))*s.y; m.v[6]=(2*(y*z+x*w))*s.y;
  m.v[8]=(2*(x*z+y*w))*s.z; m.v[9]=(2*(y*z-x*w))*s.z; m.v[10]=(1-2*(x*x+y*y))*s.z;
  m.v[12]=p.x;m.v[13]=p.y;m.v[14]=p.z;return m;
}
inline Mat4 perspective(float fovy,float aspect,float near_z,float far_z){
  Mat4 m{};const float f=1.0f/std::tan(fovy*.5f);
  m.v[0]=f/aspect;m.v[5]=-f;m.v[10]=far_z/(near_z-far_z);m.v[11]=-1;
  m.v[14]=(far_z*near_z)/(near_z-far_z);return m;
}
inline Mat4 look_at(Vec3 eye,Vec3 target,Vec3 up){
  const Vec3 f=normalize(target-eye), s=normalize(cross(f,up)), u=cross(s,f);
  Mat4 m=Mat4::identity();
  m.v[0]=s.x;m.v[4]=s.y;m.v[8]=s.z;
  m.v[1]=u.x;m.v[5]=u.y;m.v[9]=u.z;
  m.v[2]=-f.x;m.v[6]=-f.y;m.v[10]=-f.z;
  m.v[12]=-dot(s,eye);m.v[13]=-dot(u,eye);m.v[14]=dot(f,eye);return m;
}
inline Quat slerp(Quat a,Quat b,float t){
  float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
  if(d<0){b={-b.x,-b.y,-b.z,-b.w};d=-d;}
  if(d>.9995f){Quat q{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z),a.w+t*(b.w-a.w)};float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
  const float th=std::acos(std::clamp(d,-1.0f,1.0f)),sn=std::sin(th);
  const float x=std::sin((1-t)*th)/sn,y=std::sin(t*th)/sn;
  return {a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y,a.w*x+b.w*y};
}
}
