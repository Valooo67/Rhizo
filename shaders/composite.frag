#version 450
layout(set=0,binding=0) uniform sampler2D hdrColor;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;
vec3 aces(vec3 x){return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);}
void main(){vec4 hdr=texture(hdrColor,uv);vec3 mapped=aces(max(hdr.rgb,vec3(0.0)));mapped=pow(mapped,vec3(1.0/2.2));outColor=vec4(mapped,hdr.a);}
