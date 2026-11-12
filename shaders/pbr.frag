#version 450
layout(location=0) in vec3 worldNormal; layout(location=1) in vec3 worldPosition; layout(location=2) in vec4 baseColor; layout(location=3) in vec2 materialData; layout(location=0) out vec4 outColor;
const float PI=3.14159265359;
float Dggx(vec3 N,vec3 H,float r){float a=r*r,a2=a*a,n=max(dot(N,H),0.0),d=n*n*(a2-1.0)+1.0;return a2/max(PI*d*d,0.0001);}
float Gschlick(float n,float r){float k=(r+1.0)*(r+1.0)/8.0;return n/(n*(1.0-k)+k);}
vec3 Fschlick(float h,vec3 f0){return f0+(1.0-f0)*pow(clamp(1.0-h,0.0,1.0),5.0);}
void main(){vec3 N=normalize(worldNormal),V=normalize(vec3(2.8,3.0,4.5)-worldPosition),L=normalize(vec3(-0.45,0.85,0.35)),H=normalize(V+L);float m=clamp(materialData.x,0.0,1.0),r=clamp(materialData.y,0.045,1.0);vec3 albedo=max(baseColor.rgb,vec3(0.0)),f0=mix(vec3(0.04),albedo,m),F=Fschlick(max(dot(H,V),0.0),f0);float D=Dggx(N,H,r),G=Gschlick(max(dot(N,V),0.0),r)*Gschlick(max(dot(N,L),0.0),r);vec3 spec=(D*G*F)/max(4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0),0.001),kd=(1.0-F)*(1.0-m);vec3 color=(kd*albedo/PI+spec)*vec3(3.2,3.0,2.7)*max(dot(N,L),0.0)+albedo*vec3(0.12,0.14,0.18)*(1.0-m*0.65);color=color/(color+1.0);outColor=vec4(pow(color,vec3(1.0/2.2)),baseColor.a);}