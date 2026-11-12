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
uint16_t u16(const std::vector<unsigned char>&b,size_t p){return uint16_t(b[p])|uint16_t(b[p+1])<<8;}
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
  const uint32_t actual=crc32(0,reinterpret_cast<const Bytef*>(out.data()),static_cast<uInt>(out.size()));
  if(actual!=crc){error="ZIP entry CRC mismatch: "+name;return false;}
  const fs::path target=dest/fs::path(name);fs::create_directories(target.parent_path(),ec);
  std::ofstream o(target,std::ios::binary);if(!o){error="Cannot write extracted ZIP entry";return false;}
  o.write(reinterpret_cast<const char*>(out.data()),static_cast<std::streamsize>(out.size()));
 }
 return true;
}
Vec3 vec(aiVector3D v){return {v.x,v.y,v.z};}
Quat quat(aiQuaternion q){return {q.x,q.y,q.z,q.w};}
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
void import_node(Scene&out,const aiScene*in,const aiNode*node,EntityId parent,uint32_t mesh_base,ImportReport&report){
 EntityId id=out.create(node->mName.C_Str(),parent);++report.nodes;
 aiVector3D scale,position;aiQuaternion rotation;node->mTransformation.Decompose(scale,rotation,position);
 if(auto*n=out.get(id)){n->local.position=vec(position);n->local.rotation=quat(rotation);n->local.scale=vec(scale);}
 for(unsigned i=0;i<node->mNumMeshes;i++)if(node->mMeshes[i]<in->mNumMeshes)out.get(id)->mesh_indices.push_back(mesh_base+node->mMeshes[i]);
 for(unsigned i=0;i<node->mNumChildren;i++)import_node(out,in,node->mChildren[i],id,mesh_base,report);
}
}
bool import_model(Scene&scene,const std::string&path,const std::string&cache,ImportReport&report,std::string&error){
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
  aiString tex;
  if(src->GetTexture(aiTextureType_BASE_COLOR,0,&tex)==AI_SUCCESS||src->GetTexture(aiTextureType_DIFFUSE,0,&tex)==AI_SUCCESS)m.base_color_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  if(src->GetTexture(aiTextureType_NORMALS,0,&tex)==AI_SUCCESS||src->GetTexture(aiTextureType_HEIGHT,0,&tex)==AI_SUCCESS)m.normal_texture=resolve_texture(in,actual.parent_path(),fs::path(cache),tex);
  if(src->GetTexture(aiTextureType_METALNESS,0,&tex)==AI_SUCCESS)m.metallic_roughness_texture=(actual.parent_path()/tex.C_Str()).lexically_normal().string();
  if(src->GetTexture(aiTextureType_EMISSIVE,0,&tex)==AI_SUCCESS)m.emissive_texture=(actual.parent_path()/tex.C_Str()).lexically_normal().string();
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
  for(unsigned f=0;f<src->mNumFaces;f++){const aiFace&face=src->mFaces[f];for(unsigned j=0;j<face.mNumIndices;j++)mesh.indices.push_back(face.mIndices[j]);}
  scene.meshes.push_back(std::move(mesh));++report.meshes;
 }
 report.model_path=actual.string();import_node(scene,in,in->mRootNode,kInvalidEntity,mesh_base,report);
 for(size_t i=mesh_base;i<scene.meshes.size();i++)scene.meshes[i].material_index=std::max(scene.meshes[i].material_index,material_base);
 return true;
}
}
