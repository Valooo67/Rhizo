#version 450
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inColor;
layout(location=3) in vec2 inMaterial;
layout(push_constant) uniform DrawConstants { mat4 viewProjection; mat4 model; } pc;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec3 worldPosition;
layout(location=2) out vec4 baseColor;
layout(location=3) out vec2 materialData;
void main() {
 vec4 world=pc.model*vec4(inPosition,1.0);
 worldPosition=world.xyz;
 worldNormal=normalize(transpose(inverse(mat3(pc.model)))*inNormal);
 baseColor=inColor; materialData=inMaterial;
 gl_Position=pc.viewProjection*world;
}