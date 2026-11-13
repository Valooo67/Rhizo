#include "rhizo/model_importer.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace fs=std::filesystem;
namespace rhizo {
namespace {
uint16_t u16(const std::vector<unsigned char>&b,size_t p){return static_cast<uint16_t>(uint16_t(b[p])|static_cast<uint16_t>(uint16_t(b[p+1])<<8));}
uint32_t u32(const std::vector<unsigned char>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
bool safe_name(const std::string&s){if(s.empty()||s[0]=='/'||s.find('\\')!=std::string::npos)return false;fs::path p(s);for(const auto&c:p)if(c=="..")return false;return true;}
bool extract_zip(const fs::path&zip,const fs::path&dest,std::string&error){
 std::ifstream f(zip,std::ios::binary);if(!f){error="Cannot open ZIP archive";return false;}
 std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});
 if(b.size()<22){error="ZIP archive is truncated";return false;}
 size_t eocd=std::string::npos;const size_t begin=b.size()>65557?b.size()-65557:0;
 for(size_t p=b.size()-22;;--p){if(u32(b,p)==0x06054b50u){eocd=p;break;}if(p==begin)break;}
 if(eocd==std::string::npos){error="ZIP end-of-central-directory not found";return false;}
 const uint16_t count=u16(b,eocd+10);const uint32_t cd_size=u32(b,eocd+12),cd_off=u32(b,eocd+16);
 if(uint64_t(cd_off)+cd_size>b.size()){error="ZIP central directory is invalid";return false;}
 size_t p=cd_off;std::error_code ec;fs::create_directories(dest,ec);
 for(uint32_t i=0;i<count;i++){
  if(p+46>b.size()||u32(b,p)!=0x02014b50u){error="Invalid ZIP central directory entry";return false;}
  const uint16_t method=u16(b,p+10),name_len=u16(b,p+28),extra_len=u16(b,p+30),comment_len=u16(b,p+32);
  const uint32_t crc=u32(b,p+16),compressed=u32(b,p+20),uncompressed=u32(b,p+24),local=u32(b,p+42);
  if(p+46ull+name_len+extra_len+comment_len>b.size()){error="ZIP entry metadata is truncated";return false;}
  std::string name(reinterpret_cast<const char*>(b.data()+p+46),name_len);p+=46ull+name_len+extra_len+comment_len;
  if(!safe_name(name))continue;
  if(name.back()=='/'){fs::create_directories(dest/fs::path(name),ec);continue;}
  if(uncompressed>512u*1024u*1024u||uint64_t(local)+30>b.size()||u32(b,local)!=0x04034b50u){error="Invalid ZIP local file header";return false;}
  const size_t data=local+30ull+u16(b,local+26)+u16(b,local+28);
  if(data+compressed>b.size()){error="ZIP entry data is truncated";return false;}
  std::vector<unsigned char>out(uncompressed);
  if(method==0){if(compressed!=uncompressed){error="Invalid stored ZIP entry";return false;}std::copy_n(b.data()+data,uncompressed,out.data());}
  else if(method==8){
   z_stream z{};z.next_in=const_cast<Bytef*>(reinterpret_cast<const Bytef*>(b.data()+data));z.avail_in=compressed;
   z.next_out=reinterpret_cast<Bytef*>(out.data());z.avail_out=uncompressed;
   if(inflateInit2(&z,-MAX_WBITS)!=Z_OK){error="ZIP inflate initialization failed";return false;}
   const int rc=inflate(&z,Z_FINISH);inflateEnd(&z);if(rc!=Z_STREAM_END||z.total_out!=uncompressed){error="ZIP deflate stream is invalid";return false;}
  }else continue;
  const uLong actual=crc32(0,reinterpret_cast<const Bytef*>(out.data()),static_cast<uInt>(out.size()));
  if(actual!=crc){error="ZIP entry CRC mismatch: "+name;return false;}
  const fs::path target=dest/fs::path(name);fs::create_directories(target.parent_path(),ec);
  std::ofstream o(target,std::ios::binary);if(!o){error="Cannot write extracted ZIP entry";return false;}
  o.write(reinterpret_cast<const char*>(out.data()),static_cast<std::streamsize>(out.size()));
 }
 return true;
}
Vec3 vec(aiVector3D v){return {v.x,v.y,v.z};}
Quat quat(aiQuaternion q){return {q.x,q.y,q.z,q.w};}\nMat4 mat4(const aiMatrix4x4&m){Mat4 o;o.v={m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4};return o;}
std::string resolve_texture(const aiScene*scene,const fs::path&model_dir,const fs::path&cache,const aiString&texture){
 std::string raw=texture.C_Str();
 if(raw.empty())return {};
 if(raw[0]!='*')return (model_dir/fs::path(raw)).lexically_normal().string();
 unsigned index=0;try{index=static_cast<unsigned>(std::stoul(raw.substr(1)));}catch(...){return raw;}
 if(index>=scene->mNumTextures)return raw;
 const aiTexture*t=scene->mTextures[index];fs::path dir=cache/"rhizo_embedded_textures";
 std::error_code ec;fs::create_directories(dir,ec);
 std::string ext=t->mHeight==0?(t->achFormatHint[0]?t->achFormatHint:"bin"):"tga";
 fs::path file=dir/("texture_"+std::to_string(index)+"."+ext);
 if(fs::exists(file))return file.string();
 std::ofstream out(file,std::ios::binary);if(!out)return raw;
 if(t->mHeight==0){out.write(reinterpret_cast<const char*>(t->pcData),static_cast<std::streamsize>(t->mWidth));}
 else{
  const uint16_t w=static_cast<uint16_t>(std::min(t->mWidth,65535u)),h=static_cast<uint16_t>(std::min(t->mHeight,65535u));
  unsigned char header[18]={0,0,2,0,0,0,0,0,0,0,0,0,static_cast<unsigned char>(w&255),static_cast<unsigned char>(w>>8),static_cast<unsigned char>(h&255),static_cast<unsigned char>(h>>8),32,8};
  out.write(reinterpret_cast<const char*>(header),18);
  for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++){const aiTexel&px=t->pcData[y*t->mWidth+x];const unsigned char bgra[4]={px.b,px.g,px.r,px.a};out.write(reinterpret_cast<const char*>(bgra),4);}
 }
 return file.string();
}
void import_node(Scene&out,const aiScene*in,const aiNode*node,EntityId parent,uint32_t mesh_base,ImportReport&report,std::unordered_map<std::string,EntityId>&node_ids){
 EntityId id=out.create(node->mName.C_Str(),parent);++report.nodes;if(id)node_ids.try_emplace(node->mName.C_Str(),id);
 aiVector3D scale,position;aiQuaternion rotation;node->mTransformation.Decompose(scale,rotation,position);
 if(auto*n=out.get(id)){n->local.position=vec(position);n->local.rotation=quat(rotation);n->local.scale=vec(scale);}
 for(unsigned i=0;i<node->mNumMeshes;i++)if(node->mMeshes[i]<in->mNumMeshes){const uint32_t meshIndex=mesh_base+node->mMeshes[i];out.get(id)->mesh_indices.push_back(meshIndex);const size_t morphCount=out.meshes[meshIndex].morph_targets.size();if(out.get(id)->morph_weights.size()<morphCount)out.get(id)->morph_weights.resize(morphCount,0.0f);}
 for(unsigned i=0;i<node->mNumChildren;i++)import_node(out,in,node->mChildren[i],id,mesh_base,report,node_ids);
}
}
bool import_model(Scene&scene,const std::string&path,const std::string&cache,ImportReport&report,std::string&error,std::vector<AnimationClip>*animations){
 fs::path source(path),actual=source;std::error_code ec;
 if(!fs::exists(source)){error="Model file does not exist: "+path;return false;}
 if(source.extension()==".zip"||source.extension()==".ZIP"){
  fs::path unpack=fs::path(cache)/("rhizo_import_"+std::to_string(std::hash<std::string>{}(path)));
  if(!extract_zip(source,unpack,error))return false;
  std::vector<fs::path> candidates;
  for(auto it=fs::recursive_directory_iterator(unpack,ec);!ec&&it!=fs::recursive_directory_iterator();++it){
   if(!it->is_regular_file())continue;std::string ext=it->path().extension().string();
   std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
   if(ext==".fbx"||ext==".glb"||ext==".gltf"||ext==".obj"||ext==".dae")candidates.push_back(it->path());
  }
  if(candidates.empty()){error="ZIP contains no supported model (FBX, GLB, glTF, OBJ or DAE)";return false;}
  std::sort(candidates.begin(),candidates.end(),[](const auto&a,const auto&b){auto rank=[](const fs::path&p){const auto e=p.extension().string();return e==".glb"?0:e==".fbx"?1:e==".gltf"?2:e==".obj"?3:4;};return rank(a)<rank(b);});
  actual=candidates.front();
 }
 Assimp::Importer importer;
 const aiScene*in=importer.ReadFile(actual.string(),aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_CalcTangentSpace|aiProcess_JoinIdenticalVertices|aiProcess_ImproveCacheLocality|aiProcess_ValidateDataStructure|aiProcess_SortByPType);
 if(!in||!in->mRootNode){error=importer.GetErrorString();return false;}
 const uint32_t material_base=static_cast<uint32_t>(scene.materials.size());
 for(unsigned i=0;i<in->mNumMaterials;i++){
  Material m;aiMaterial*src=in->mMaterials[i];aiColor4D c;
  if(aiGetMaterialColor(src,AI_MATKEY_BASE_COLOR,&c)==AI_SUCCESS)m.base_color={c.r,c.g,c.b,c.a};
  else if(aiGetMaterialColor(src,AI_MATKEY_COLOR_DIFFUSE,&c)==AI_SUCCESS)m.base_color={c.r,c.g,c.b,c.a};
  float f=0;if(src->Get(AI_MATKEY_METALLIC_FACTOR,f)==AI_SUCCESS)m.metallic=f;
  if(src->Get(AI_MATKEY_ROUGHNESS_FACTOR,f)==AI_SUCCESS)m.roughness=f;
  aiColor3D emissiveColor;if(src->Get(AI_MATKEY_COLOR_EMISSIVE,emissiveColor)==AI_SUCCESS)m.emissive_color={emissiveColor.r,emissiveColor.g,emissiveColor.b};
  aiString tex;
  if(src->GetTexture(aiTextureType_BASE_COLOR,0,&tex)==AI_SUCCESS||src->GetTexture(aiTextureType_DIFFUSE,0,&tex)==AI_SUCCESS)m.base_color_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  if(src->GetTexture(aiTextureType_NORMALS,0,&tex)==AI_SUCCESS||src->GetTexture(aiTextureType_HEIGHT,0,&tex)==AI_SUCCESS)m.normal_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  if(src->GetTexture(aiTextureType_METALNESS,0,&tex)==AI_SUCCESS)m.metallic_roughness_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  if(src->GetTexture(aiTextureType_EMISSIVE,0,&tex)==AI_SUCCESS)m.emissive_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  scene.materials.push_back(std::move(m));++report.materials;
 }
 const uint32_t mesh_base=static_cast<uint32_t>(scene.meshes.size());
 for(unsigned i=0;i<in->mNumMeshes;i++){
  const aiMesh*src=in->mMeshes[i];Mesh mesh;mesh.name=src->mName.C_Str();mesh.material_index=material_base+src->mMaterialIndex;
  mesh.vertices.reserve(src->mNumVertices);
  for(unsigned v=0;v<src->mNumVertices;v++){
   Vertex x;x.position=vec(src->mVertices[v]);if(src->HasNormals())x.normal=vec(src->mNormals[v]);
   if(src->HasTextureCoords(0))x.uv={src->mTextureCoords[0][v].x,src->mTextureCoords[0][v].y};
   if(src->HasTangentsAndBitangents()){const auto t=src->mTangents[v];x.tangent={t.x,t.y,t.z,1};}
   mesh.vertices.push_back(x);
  }
  if(src->mNumBones>256){error="Mesh exceeds RHIZO's 256-bone skinning limit";return false;}mesh.bones.reserve(src->mNumBones);
  for(unsigned bi=0;bi<src->mNumBones;bi++){
   const aiBone*b=src->mBones[bi];MeshBone bone;bone.name=b->mName.C_Str();bone.inverse_bind=mat4(b->mOffsetMatrix);mesh.bones.push_back(std::move(bone));++report.bones;
   for(unsigned wi=0;wi<b->mNumWeights;wi++){const aiVertexWeight&w=b->mWeights[wi];if(w.mVertexId>=mesh.vertices.size()||!std::isfinite(w.mWeight)||w.mWeight<=0)continue;Vertex&v=mesh.vertices[w.mVertexId];float weights[4]={v.bone_weights.x,v.bone_weights.y,v.bone_weights.z,v.bone_weights.w};uint32_t indices[4]={v.bone_indices[0],v.bone_indices[1],v.bone_indices[2],v.bone_indices[3]};uint32_t slot=0;for(uint32_t k=1;k<4;k++)if(weights[k]<weights[slot])slot=k;if(w.mWeight>weights[slot]){weights[slot]=w.mWeight;indices[slot]=bi;v.bone_weights={weights[0],weights[1],weights[2],weights[3]};for(uint32_t k=0;k<4;k++)v.bone_indices[k]=indices[k];}}
  }
  for(auto&v:mesh.vertices){const float total=v.bone_weights.x+v.bone_weights.y+v.bone_weights.z+v.bone_weights.w;if(total>1.0e-8f){v.bone_weights.x/=total;v.bone_weights.y/=total;v.bone_weights.z/=total;v.bone_weights.w/=total;}}
  if(src->mNumAnimMeshes>32){error="Mesh exceeds RHIZO's 32 morph-target limit";return false;}mesh.morph_targets.reserve(src->mNumAnimMeshes);
  for(unsigned mi=0;mi<src->mNumAnimMeshes;mi++){const aiAnimMesh*morph=src->mAnimMeshes[mi];MorphTarget target;target.name=morph->mName.C_Str();target.position_deltas.resize(src->mNumVertices);target.normal_deltas.resize(src->mNumVertices);for(unsigned vi=0;vi<src->mNumVertices;vi++){if(morph->mVertices)target.position_deltas[vi]=vec(morph->mVertices[vi])-mesh.vertices[vi].position;if(morph->mNormals&&src->HasNormals())target.normal_deltas[vi]=vec(morph->mNormals[vi])-mesh.vertices[vi].normal;}mesh.morph_targets.push_back(std::move(target));++report.morph_targets;}

  for(unsigned f=0;f<src->mNumFaces;f++){const aiFace&face=src->mFaces[f];for(unsigned j=0;j<face.mNumIndices;j++)mesh.indices.push_back(face.mIndices[j]);}
  scene.meshes.push_back(std::move(mesh));++report.meshes;
 }
 report.model_path=actual.string();std::unordered_map<std::string,EntityId>node_ids;import_node(scene,in,in->mRootNode,kInvalidEntity,mesh_base,report,node_ids);
 for(size_t i=mesh_base;i<scene.meshes.size();i++){scene.meshes[i].material_index=std::max(scene.meshes[i].material_index,material_base);for(auto&bone:scene.meshes[i].bones){auto it=node_ids.find(bone.name);if(it!=node_ids.end())bone.node=it->second;}}
 if(animations){animations->clear();animations->reserve(in->mNumAnimations);for(unsigned ai=0;ai<in->mNumAnimations;ai++){const aiAnimation*src=in->mAnimations[ai];AnimationClip clip;clip.name=src->mName.length?src->mName.C_Str():"Animation_"+std::to_string(ai);const double ticks=src->mTicksPerSecond>0?src->mTicksPerSecond:25.0;clip.animation.set_duration(src->mDuration/ticks);
   for(unsigned ci=0;ci<src->mNumChannels;ci++){const aiNodeAnim*channel=src->mChannels[ci];auto found=node_ids.find(channel->mNodeName.C_Str());if(found==node_ids.end())continue;const EntityId entity=found->second;
    for(unsigned k=0;k<channel->mNumPositionKeys;k++){Transform t;t.position=vec(channel->mPositionKeys[k].mValue);clip.animation.set_key(entity,TrackProperty::Position,channel->mPositionKeys[k].mTime/ticks,t);}
    for(unsigned k=0;k<channel->mNumRotationKeys;k++){Transform t;t.rotation=quat(channel->mRotationKeys[k].mValue);clip.animation.set_key(entity,TrackProperty::Rotation,channel->mRotationKeys[k].mTime/ticks,t);}
    for(unsigned k=0;k<channel->mNumScalingKeys;k++){Transform t;t.scale=vec(channel->mScalingKeys[k].mValue);clip.animation.set_key(entity,TrackProperty::Scale,channel->mScalingKeys[k].mTime/ticks,t);}
   }
   for(unsigned ci=0;ci<src->mNumMorphMeshChannels;ci++){const aiMeshMorphAnim*channel=src->mMorphMeshChannels[ci];for(size_t meshOffset=0;meshOffset<scene.meshes.size()-mesh_base;meshOffset++){const uint32_t meshIndex=mesh_base+static_cast<uint32_t>(meshOffset);const Mesh&mesh=scene.meshes[meshIndex];if(mesh.name!=channel->mName.C_Str())continue;for(const auto&node:scene.nodes()){if(std::find(node.mesh_indices.begin(),node.mesh_indices.end(),meshIndex)==node.mesh_indices.end())continue;for(unsigned keyIndex=0;keyIndex<channel->mNumKeys;keyIndex++){const aiMeshMorphKey&key=channel->mKeys[keyIndex];for(unsigned valueIndex=0;valueIndex<key.mNumValuesAndWeights;valueIndex++){const uint32_t target=key.mValues[valueIndex];if(target>=mesh.morph_targets.size()||target>=32)continue;clip.animation.set_parameter_key(node.id,17u+target,key.mTime/ticks,{static_cast<float>(key.mWeights[valueIndex]),0,0,0});}}}}}
   if(clip.animation.key_count()>0||clip.animation.duration()>0)animations->push_back(std::move(clip));
  }report.animations=static_cast<uint32_t>(animations->size());}
 return true;
}
}
