#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace rhizo {
class ComponentRegistry {
  struct IStorage {
    virtual ~IStorage()=default;
    virtual void erase(uint32_t)=0;
    virtual bool contains(uint32_t)const=0;
    virtual std::unique_ptr<IStorage> clone()const=0;
  };
  template<class T> struct Storage final:IStorage {
    std::unordered_map<uint32_t,T> values;
    void erase(uint32_t id)override{values.erase(id);}
    bool contains(uint32_t id)const override{return values.contains(id);}
    std::unique_ptr<IStorage> clone()const override{return std::make_unique<Storage<T>>(*this);}
  };
  std::unordered_map<std::type_index,std::unique_ptr<IStorage>> pools_;
  template<class T> Storage<T>* find(){auto i=pools_.find(typeid(T));return i==pools_.end()?nullptr:static_cast<Storage<T>*>(i->second.get());}
  template<class T> const Storage<T>* find()const{auto i=pools_.find(typeid(T));return i==pools_.end()?nullptr:static_cast<const Storage<T>*>(i->second.get());}
 public:
  ComponentRegistry()=default;
  ComponentRegistry(const ComponentRegistry& other){for(const auto&[type,pool]:other.pools_)pools_.emplace(type,pool->clone());}
  ComponentRegistry& operator=(const ComponentRegistry& other){if(this!=&other){ComponentRegistry copy(other);pools_.swap(copy.pools_);}return *this;}
  ComponentRegistry(ComponentRegistry&&)noexcept=default;
  ComponentRegistry& operator=(ComponentRegistry&&)noexcept=default;
  template<class T,class...A>T& emplace(uint32_t id,A&&...args){auto*p=find<T>();if(!p)p=static_cast<Storage<T>*>(pools_.emplace(typeid(T),std::make_unique<Storage<T>>()).first->second.get());return p->values.insert_or_assign(id,T(std::forward<A>(args)...)).first->second;}
  template<class T>T* get(uint32_t id){auto*p=find<T>();if(!p)return nullptr;auto i=p->values.find(id);return i==p->values.end()?nullptr:&i->second;}
  template<class T>const T* get(uint32_t id)const{auto*p=find<T>();if(!p)return nullptr;auto i=p->values.find(id);return i==p->values.end()?nullptr:&i->second;}
  template<class T>bool has(uint32_t id)const{auto*p=find<T>();return p&&p->contains(id);}
  template<class T>bool remove(uint32_t id){auto*p=find<T>();return p&&p->values.erase(id)>0;}
  void remove_entity(uint32_t id){for(auto&[type,pool]:pools_){(void)type;pool->erase(id);}}
  void clear(){pools_.clear();}
  std::size_t component_type_count()const{return pools_.size();}
};
}
