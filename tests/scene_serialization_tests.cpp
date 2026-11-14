#include "rhizo/scene_io.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace rhizo;

int main(){
 Scene scene;const EntityId root=scene.create("RigRoot");const EntityId removed=scene.create("Temporary");assert(scene.destroy(removed));const EntityId bone=scene.create("Bone",root);assert(root&&bone);
 Mesh mesh;mesh.name="SkinnedMesh";Vertex vertex;vertex.bone_indices[0]=0;vertex.bone_weights={1,0,0,0};mesh.vertices.push_back(vertex);mesh.indices={0};MeshBone binding;binding.name="Bone";binding.node=bone;mesh.bones.push_back(binding);MorphTarget morph;morph.name="Smile";morph.position_deltas={{0.1f,0,0}};morph.normal_deltas={{0,0.1f,0}};mesh.morph_targets.push_back(morph);scene.meshes.push_back(mesh);scene.get(root)->mesh_indices.push_back(0);scene.get(root)->morph_weights={0.65f};
 scene.components.emplace<CustomComponentSet>(root).values[77]={1,2,3,4};DecalComponent decal;decal.color={0.2f,0.4f,0.8f,0.75f};decal.opacity=0.65f;decal.angle_fade=0.3f;auto&decalBytes=scene.components.get<CustomComponentSet>(root)->values[kDecalComponentType];decalBytes.resize(sizeof(decal));std::memcpy(decalBytes.data(),&decal,sizeof(decal));
 std::string error;const char*scenePath="/tmp/rhizo-scene-v4-test.rhizoscene";assert(save_scene(scene,scenePath,error));Scene loaded;assert(load_scene(loaded,scenePath,error));assert(loaded.meshes.size()==1);assert(loaded.meshes[0].bones.size()==1);assert(loaded.meshes[0].bones[0].node==2);assert(loaded.meshes[0].morph_targets.size()==1);assert(loaded.meshes[0].vertices[0].bone_weights.x==1);assert(loaded.get(1)->morph_weights.size()==1);assert(loaded.get(1)->morph_weights[0]>0.64f);assert(loaded.components.get<CustomComponentSet>(1)->values.at(77).size()==4);const auto&loadedDecalBytes=loaded.components.get<CustomComponentSet>(1)->values.at(kDecalComponentType);assert(loadedDecalBytes.size()==sizeof(DecalComponent));DecalComponent loadedDecal{};std::memcpy(&loadedDecal,loadedDecalBytes.data(),sizeof(loadedDecal));assert(loadedDecal.opacity>0.64f&&loadedDecal.angle_fade>0.29f);
 const char*prefabPath="/tmp/rhizo-prefab-v4-test.rhizoprefab";assert(save_prefab(scene,root,prefabPath,error));Scene host;const EntityId hostRoot=host.create("Host");EntityId instance=0;assert(instantiate_prefab(host,prefabPath,hostRoot,instance,error));assert(instance!=0);assert(host.get(instance)->name=="RigRoot");EntityId importedBone=0;for(const auto&node:host.nodes())if(node.name=="Bone")importedBone=node.id;assert(importedBone!=0);assert(host.meshes[0].bones[0].node==importedBone);assert(host.get(instance)->morph_weights.size()==1);const auto*hostCustom=host.components.get<CustomComponentSet>(instance);assert(hostCustom&&hostCustom->values.at(kDecalComponentType).size()==sizeof(DecalComponent));
 std::remove(scenePath);std::remove(prefabPath);return 0;
}
