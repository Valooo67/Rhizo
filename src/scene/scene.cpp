#include "rhizo/scene.hpp"
#include <algorithm>
namespace rhizo {
EntityId Scene::create(std::string name,EntityId parent){
  if(parent!=kInvalidEntity&&!get(parent))return kInvalidEntity;
  SceneNode n;n.id=next_id_++;n.name=std::move(name);n.parent=parent;
  nodes_.push_back(std::move(n));
  if(parent!=kInvalidEntity)get(parent)->children.push_back(nodes_.back().id);
  return nodes_.back().id;
}
SceneNode* Scene::get(EntityId id){for(auto&n:nodes_)if(n.id==id)return &n;return nullptr;}
const SceneNode* Scene::get(EntityId id)const{for(const auto&n:nodes_)if(n.id==id)return &n;return nullptr;}
bool Scene::set_parent(EntityId id,EntityId parent){
  auto*n=get(id);if(!n||id==parent||(parent!=kInvalidEntity&&!get(parent)))return false;
  for(EntityId p=parent;p!=kInvalidEntity;){if(p==id)return false;const auto*q=get(p);if(!q)break;p=q->parent;}
  if(n->parent!=kInvalidEntity)if(auto*old=get(n->parent)){auto&v=old->children;v.erase(std::remove(v.begin(),v.end(),id),v.end());}
  n->parent=parent;if(parent!=kInvalidEntity)get(parent)->children.push_back(id);return true;
}
bool Scene::destroy(EntityId id){
  auto*n=get(id);if(!n)return false;
  const auto children=n->children;for(EntityId c:children)destroy(c);
  if(n->parent!=kInvalidEntity)if(auto*p=get(n->parent)){auto&v=p->children;v.erase(std::remove(v.begin(),v.end(),id),v.end());}
  components.remove_entity(id);
  nodes_.erase(std::remove_if(nodes_.begin(),nodes_.end(),[id](const SceneNode&x){return x.id==id;}),nodes_.end());return true;
}
Mat4 Scene::world_matrix(EntityId id)const{
  const auto*n=get(id);if(!n)return Mat4::identity();
  const Mat4 local=compose(n->local.position,n->local.rotation,n->local.scale);
  return n->parent==kInvalidEntity?local:multiply(world_matrix(n->parent),local);
}
std::vector<EntityId> Scene::roots()const{std::vector<EntityId> r;for(const auto&n:nodes_)if(n.parent==kInvalidEntity)r.push_back(n.id);return r;}
void Scene::clear(){nodes_.clear();meshes.clear();materials.clear();components.clear();next_id_=1;}
}
