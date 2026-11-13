#include "rhizo/scene_io.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace rhizo { namespace {
constexpr uint32_t kVersion=4,kMaxNodes=1000000,kMaxMeshes=100000,kMaxVertices=10000000,kMaxString=1u<<20,kMaxComponents=4096,kMaxComponentBytes=1u<<20;
struct Writer{
 std::ofstream f;explicit Writer(const std::string&p):f(p,std::ios::binary|std::ios::trunc){}
 template<class T>bool pod(T v){static_assert(std::is_arithmetic_v<T>);f.write(reinterpret_cast<const char*>(&v),sizeof(v));return bool(f);}
 bool str(const std::string&s){if(s.size()>kMaxString)return false;return pod(uint32_t(s.size()))&&(f.write(s.data(),static_cast<std::streamsize>(s.size())),bool(f));}
};
struct Reader{
 std::ifstream f;explicit Reader(const std::string&p):f(p,std::ios::binary){}
 template<class T>bool pod(T&v){static_assert(std::is_arithmetic_v<T>);return bool(f.read(reinterpret_cast<char*>(&v),sizeof(v)));}
 bool str(std::string&s){uint32_t n=0;if(!pod(n)||n>kMaxString)return false;s.resize(n);return n==0||bool(f.read(s.data(),n));}
};
bool write_vec3(Writer&w,Vec3 v){return w.pod(v.x)&&w.pod(v.y)&&w.pod(v.z);}
bool read_vec3(Reader&r,Vec3&v){return r.pod(v.x)&&r.pod(v.y)&&r.pod(v.z)&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool write_vec4(Writer&w,Vec4 v){return w.pod(v.x)&&w.pod(v.y)&&w.pod(v.z)&&w.pod(v.w);}
bool read_vec4(Reader&r,Vec4&v){return r.pod(v.x)&&r.pod(v.y)&&r.pod(v.z)&&r.pod(v.w)&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w);}
bool write_scene(const Scene&s,const std::string&p,const std::vector<EntityId>&ids){
 Writer w(p);if(!w.f)return false;
 if(!w.pod(uint32_t(0x4f5a4852))||!w.pod(kVersion)||!w.pod(uint32_t(ids.size()))||!w.pod(uint32_t(s.meshes.size()))||!w.pod(uint32_t(s.materials.size())))return false;
 for(const auto&m:s.materials){if(!write_vec4(w,m.base_color)||!w.pod(m.metallic)||!w.pod(m.roughness)||!w.pod(m.normal_scale)||!w.pod(m.occlusion)||!write_vec3(w,m.emissive_color)||!w.str(m.base_color_texture)||!w.str(m.normal_texture)||!w.str(m.metallic_roughness_texture)||!w.str(m.emissive_texture))return false;}
 for(const auto&m:s.meshes){if(!w.str(m.name)||!w.pod(m.material_index)||m.vertices.size()>kMaxVertices||m.indices.size()>kMaxVertices*3u||!w.pod(uint32_t(m.vertices.size()))||!w.pod(uint32_t(m.indices.size())))return false;
  for(const auto&v:m.vertices)if(!write_vec3(w,v.position)||!write_vec3(w,v.normal)||!w.pod(v.uv.x)||!w.pod(v.uv.y)||!write_vec4(w,v.tangent)||!w.pod(v.bone_indices[0])||!w.pod(v.bone_indices[1])||!w.pod(v.bone_indices[2])||!w.pod(v.bone_indices[3])||!write_vec4(w,v.bone_weights))return false;
  for(uint32_t i:m.indices)if(!w.pod(i))return false;
  if(m.bones.size()>256||m.morph_targets.size()>256||!w.pod(uint32_t(m.bones.size())))return false;for(const auto&b:m.bones){if(!w.str(b.name)||!w.pod(b.node))return false;for(float x:b.inverse_bind.v)if(!w.pod(x))return false;}if(!w.pod(uint32_t(m.morph_targets.size())))return false;for(const auto&t:m.morph_targets){if(t.position_deltas.size()!=m.vertices.size()||t.normal_deltas.size()!=m.vertices.size()||!w.str(t.name))return false;for(size_t vi=0;vi<t.position_deltas.size();vi++)if(!write_vec3(w,t.position_deltas[vi])||!write_vec3(w,t.normal_deltas[vi]))return false;}
 }
 for(EntityId id:ids){const auto*n=s.get(id);if(!n)return false;
  const bool parentIncluded=n->parent!=kInvalidEntity&&std::find(ids.begin(),ids.end(),n->parent)!=ids.end();const EntityId savedParent=parentIncluded?n->parent:kInvalidEntity;
  if(!w.pod(n->id)||!w.pod(savedParent)||!w.str(n->name)||!write_vec3(w,n->local.position)||!w.pod(n->local.rotation.x)||!w.pod(n->local.rotation.y)||!w.pod(n->local.rotation.z)||!w.pod(n->local.rotation.w)||!write_vec3(w,n->local.scale)||!w.pod(uint8_t(n->visible))||!w.pod(uint8_t(n->has_camera))||!w.pod(uint8_t(n->has_light))||!w.pod(uint32_t(n->mesh_indices.size())))return false;
  for(uint32_t i:n->mesh_indices)if(!w.pod(i))return false;
  const auto&c=n->camera;if(!w.pod(c.vertical_fov)||!w.pod(c.near_plane)||!w.pod(c.far_plane)||!w.pod(c.orthographic_size)||!w.pod(uint8_t(c.orthographic)))return false;
  const auto&l=n->light;if(!w.pod(uint32_t(l.type))||!write_vec3(w,l.color)||!w.pod(l.intensity)||!w.pod(l.range)||!w.pod(l.inner_cone)||!w.pod(l.outer_cone)||!w.pod(uint8_t(l.casts_shadows)))return false;
  const auto*custom=s.components.get<CustomComponentSet>(id);const uint32_t count=custom?static_cast<uint32_t>(custom->values.size()):0;if(count>kMaxComponents||!w.pod(count))return false;
  if(custom){std::vector<uint32_t>types;types.reserve(custom->values.size());for(const auto&entry:custom->values)types.push_back(entry.first);std::sort(types.begin(),types.end());for(uint32_t typeId:types){const auto&data=custom->values.at(typeId);if(typeId==0||data.size()>kMaxComponentBytes||!w.pod(typeId)||!w.pod(uint32_t(data.size())))return false;if(!data.empty())w.f.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));if(!w.f)return false;}}
  if(n->morph_weights.size()>256||!w.pod(uint32_t(n->morph_weights.size())))return false;for(float weight:n->morph_weights)if(!std::isfinite(weight)||!w.pod(weight))return false;
 }
 return bool(w.f);
}
bool read_scene(Scene&out,const std::string&p,std::string&err){
 Reader r(p);uint32_t magic=0,ver=0,nc=0,mc=0,matc=0;
 if(!r.f||!r.pod(magic)||!r.pod(ver)||!r.pod(nc)||!r.pod(mc)||!r.pod(matc)||magic!=0x4f5a4852||(ver!=3&&ver!=kVersion)||nc>kMaxNodes||mc>kMaxMeshes||matc>kMaxMeshes){err="Invalid RHIZO scene header or unsupported version";return false;}
 Scene tmp;
 for(uint32_t i=0;i<matc;i++){Material m;if(!read_vec4(r,m.base_color)||!r.pod(m.metallic)||!r.pod(m.roughness)||!r.pod(m.normal_scale)||!r.pod(m.occlusion)||!read_vec3(r,m.emissive_color)||!r.str(m.base_color_texture)||!r.str(m.normal_texture)||!r.str(m.metallic_roughness_texture)||!r.str(m.emissive_texture)){err="Corrupt material data";return false;}tmp.materials.push_back(std::move(m));}
 for(uint32_t i=0;i<mc;i++){Mesh m;uint32_t vc=0,ic=0;if(!r.str(m.name)||!r.pod(m.material_index)||!r.pod(vc)||!r.pod(ic)||vc>kMaxVertices||ic>kMaxVertices*3u||m.material_index>=std::max(1u,matc)){err="Invalid mesh header";return false;}m.vertices.resize(vc);m.indices.resize(ic);for(auto&v:m.vertices){if(!read_vec3(r,v.position)||!read_vec3(r,v.normal)||!r.pod(v.uv.x)||!r.pod(v.uv.y)||!read_vec4(r,v.tangent)){err="Corrupt vertex data";return false;}if(ver>=4&&(!r.pod(v.bone_indices[0])||!r.pod(v.bone_indices[1])||!r.pod(v.bone_indices[2])||!r.pod(v.bone_indices[3])||!read_vec4(r,v.bone_weights))){err="Corrupt skinning vertex data";return false;}}for(auto&x:m.indices)if(!r.pod(x)||x>=vc){err="Mesh index is out of range";return false;}
  if(ver>=4){uint32_t bones=0,morphs=0;if(!r.pod(bones)||bones>256){err="Invalid mesh bone count";return false;}m.bones.resize(bones);for(auto&b:m.bones){if(!r.str(b.name)||!r.pod(b.node)){err="Corrupt bone record";return false;}for(float&x:b.inverse_bind.v)if(!r.pod(x)||!std::isfinite(x)){err="Corrupt inverse bind matrix";return false;}}if(!r.pod(morphs)||morphs>256){err="Invalid morph target count";return false;}m.morph_targets.resize(morphs);for(auto&t:m.morph_targets){if(!r.str(t.name)){err="Corrupt morph target name";return false;}t.position_deltas.resize(vc);t.normal_deltas.resize(vc);for(uint32_t vi=0;vi<vc;vi++)if(!read_vec3(r,t.position_deltas[vi])||!read_vec3(r,t.normal_deltas[vi])){err="Corrupt morph target data";return false;}}}
  tmp.meshes.push_back(std::move(m));}
 struct Pending{EntityId old,parent;SceneNode node;CustomComponentSet custom;};std::vector<Pending> pending;pending.reserve(nc);
 for(uint32_t i=0;i<nc;i++){Pending pnd{};uint8_t visible=0,camera=0,light=0,ortho=0,shadows=0;uint32_t meshes=0,type=0,componentCount=0;
  auto&n=pnd.node;if(!r.pod(pnd.old)||!r.pod(pnd.parent)||!r.str(n.name)||!read_vec3(r,n.local.position)||!r.pod(n.local.rotation.x)||!r.pod(n.local.rotation.y)||!r.pod(n.local.rotation.z)||!r.pod(n.local.rotation.w)||!read_vec3(r,n.local.scale)||!r.pod(visible)||!r.pod(camera)||!r.pod(light)||!r.pod(meshes)||meshes>mc){err="Corrupt scene node";return false;}
  n.visible=visible!=0;n.has_camera=camera!=0;n.has_light=light!=0;for(uint32_t j=0;j<meshes;j++){uint32_t x;if(!r.pod(x)||x>=mc){err="Node mesh reference is invalid";return false;}n.mesh_indices.push_back(x);}
  auto&c=n.camera;if(!r.pod(c.vertical_fov)||!r.pod(c.near_plane)||!r.pod(c.far_plane)||!r.pod(c.orthographic_size)||!r.pod(ortho)){err="Corrupt camera component";return false;}c.orthographic=ortho!=0;
  auto&l=n.light;if(!r.pod(type)||type>2||!read_vec3(r,l.color)||!r.pod(l.intensity)||!r.pod(l.range)||!r.pod(l.inner_cone)||!r.pod(l.outer_cone)||!r.pod(shadows)){err="Corrupt light component";return false;}l.type=static_cast<LightType>(type);l.casts_shadows=shadows!=0;
  if(!r.pod(componentCount)||componentCount>kMaxComponents){err="Invalid custom component count";return false;}
  for(uint32_t j=0;j<componentCount;j++){uint32_t componentType=0,bytes=0;if(!r.pod(componentType)||!r.pod(bytes)||componentType==0||bytes>kMaxComponentBytes||pnd.custom.values.contains(componentType)){err="Invalid custom component record";return false;}auto&payload=pnd.custom.values[componentType];payload.resize(bytes);if(bytes&&!r.f.read(reinterpret_cast<char*>(payload.data()),static_cast<std::streamsize>(bytes))){err="Truncated custom component payload";return false;}}
  if(ver>=4){uint32_t morphWeights=0;if(!r.pod(morphWeights)||morphWeights>256){err="Invalid node morph weight count";return false;}n.morph_weights.resize(morphWeights);for(float&weight:n.morph_weights)if(!r.pod(weight)||!std::isfinite(weight)){err="Invalid node morph weight";return false;}}
  if(!std::isfinite(n.local.rotation.x)||!std::isfinite(n.local.rotation.y)||!std::isfinite(n.local.rotation.z)||!std::isfinite(n.local.rotation.w)){err="Invalid node rotation";return false;}pending.push_back(std::move(pnd));
 }
 std::unordered_map<EntityId,EntityId> map;for(auto&x:pending){if(x.old==kInvalidEntity||map.contains(x.old)){err="Scene contains a duplicate or invalid entity ID";return false;}EntityId id=tmp.create(x.node.name);if(!id){err="Unable to allocate scene entity";return false;}map.emplace(x.old,id);}
 for(auto&x:pending){const EntityId id=map[x.old];auto*n=tmp.get(id);n->local=x.node.local;n->visible=x.node.visible;n->has_camera=x.node.has_camera;n->camera=x.node.camera;n->has_light=x.node.has_light;n->light=x.node.light;n->mesh_indices=std::move(x.node.mesh_indices);n->morph_weights=std::move(x.node.morph_weights);if(!x.custom.values.empty())tmp.components.emplace<CustomComponentSet>(id,std::move(x.custom));if(x.parent){auto it=map.find(x.parent);if(it==map.end()||!tmp.set_parent(id,it->second)){err="Scene hierarchy contains an invalid parent or cycle";return false;}}}
 out=std::move(tmp);return true;
}
void collect(const Scene&s,EntityId id,std::vector<EntityId>&out){const auto*n=s.get(id);if(!n)return;out.push_back(id);for(EntityId c:n->children)collect(s,c,out);}
}
bool save_scene(const Scene&s,const std::string&p,std::string&e){if(s.nodes().size()>kMaxNodes||s.meshes.size()>kMaxMeshes||s.materials.size()>kMaxMeshes){e="Scene exceeds supported serialization limits";return false;}std::vector<EntityId>ids;ids.reserve(s.nodes().size());for(const auto&n:s.nodes())ids.push_back(n.id);if(!write_scene(s,p,ids)){e="Failed to write scene file";return false;}return true;}
bool load_scene(Scene&s,const std::string&p,std::string&e){return read_scene(s,p,e);}
bool save_prefab(const Scene&s,EntityId root,const std::string&p,std::string&e){if(!s.get(root)){e="Prefab root entity does not exist";return false;}std::vector<EntityId>ids;collect(s,root,ids);if(!write_scene(s,p,ids)){e="Failed to write prefab";return false;}return true;}
bool instantiate_prefab(Scene&s,const std::string&p,EntityId parent,EntityId&root,std::string&e){
 if(parent&&!s.get(parent)){e="Prefab parent entity does not exist";return false;}
 Scene prefab;if(!read_scene(prefab,p,e))return false;
 Scene merged=s;
 const uint32_t meshBase=static_cast<uint32_t>(merged.meshes.size()),materialBase=static_cast<uint32_t>(merged.materials.size());
 for(const auto&m:prefab.materials)merged.materials.push_back(m);
 for(auto m:prefab.meshes){if(m.material_index<prefab.materials.size())m.material_index+=materialBase;merged.meshes.push_back(std::move(m));}
 std::unordered_map<EntityId,EntityId>map;
 for(const auto&n:prefab.nodes()){EntityId id=merged.create(n.name);if(!id){e="Failed to create prefab entity";return false;}map[n.id]=id;}
 EntityId newRoot=0;
 for(const auto&n:prefab.nodes()){
  auto*dst=merged.get(map[n.id]);if(!dst){e="Prefab entity mapping is invalid";return false;}
  dst->local=n.local;dst->visible=n.visible;dst->has_camera=n.has_camera;dst->camera=n.camera;dst->has_light=n.has_light;dst->light=n.light;dst->morph_weights=n.morph_weights;
  if(const auto*custom=prefab.components.get<CustomComponentSet>(n.id))merged.components.emplace<CustomComponentSet>(dst->id,*custom);
  for(uint32_t m:n.mesh_indices)dst->mesh_indices.push_back(meshBase+m);
  if(n.parent){auto it=map.find(n.parent);if(it==map.end()||!merged.set_parent(dst->id,it->second)){e="Prefab hierarchy is invalid";return false;}}
  else{if(!newRoot)newRoot=dst->id;if(parent&&!merged.set_parent(dst->id,parent)){e="Could not attach prefab root";return false;}}
 }
 if(!newRoot){e="Prefab has no root entity";return false;}
 s=std::move(merged);root=newRoot;return true;
}
}
