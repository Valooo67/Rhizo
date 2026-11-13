#version 450
layout(location=0) in vec3 inPosition;layout(location=7) in uvec4 inBoneIndices;layout(location=8) in vec4 inBoneWeights;\nlayout(set=0,binding=0,std430) readonly buffer SkinPalette { mat4 boneMatrices[]; } skin;
layout(push_constant) uniform ShadowConstants { mat4 lightViewProjection;mat4 model; } pc;
void main(){mat4 skinMatrix=mat4(1.0);float totalWeight=dot(inBoneWeights,vec4(1.0));if(totalWeight>0.0001){skinMatrix=skin.boneMatrices[inBoneIndices.x]*inBoneWeights.x+skin.boneMatrices[inBoneIndices.y]*inBoneWeights.y+skin.boneMatrices[inBoneIndices.z]*inBoneWeights.z+skin.boneMatrices[inBoneIndices.w]*inBoneWeights.w;skinMatrix/=totalWeight;}gl_Position=pc.lightViewProjection*pc.model*skinMatrix*vec4(inPosition,1.0);}
