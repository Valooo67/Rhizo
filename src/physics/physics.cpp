#include "rhizo/physics.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpring2Constraint.h>
#include <cmath>
#include <cstring>
#include <unordered_map>
namespace rhizo {
namespace {
constexpr uint32_t kBody=0x50485942u, kJoint=0x5048594au, kAnim=0x50485941u;
bool valid3(const float* p){return p && std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);}
btVector3 v3(const float* p){return {p[0],p[1],p[2]};}
btTransform toBt(const Transform& t){btTransform x;x.setIdentity();x.setOrigin({t.position.x,t.position.y,t.position.z});x.setRotation({t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w});return x;}
Transform fromBt(const btTransform& t){auto p=t.getOrigin();auto q=t.getRotation();Transform r;r.position={p.x(),p.y(),p.z()};r.rotation={q.x(),q.y(),q.z(),q.w()};return r;}
float volume(const RhizoRigidBodyInfo& d){float x=d.dimensions[0],y=d.dimensions[1],z=d.dimensions[2];if(d.shape_type==RHIZO_SHAPE_SPHERE)return 4.18879f*x*x*x;if(d.shape_type==RHIZO_SHAPE_CAPSULE)return 3.14159f*x*x*2*y+4.18879f*x*x*x;return 8*x*y*z;}
}
struct PhysicsWorld::Impl {
 struct Body { RhizoRigidBodyInfo desc{}; std::unique_ptr<btTriangleMesh> triangles; std::unique_ptr<btCollisionShape> shape; std::unique_ptr<btDefaultMotionState> motion; std::unique_ptr<btRigidBody> rigid; };
 std::unique_ptr<btDefaultCollisionConfiguration> config=std::make_unique<btDefaultCollisionConfiguration>();
 std::unique_ptr<btCollisionDispatcher> dispatcher=std::make_unique<btCollisionDispatcher>(config.get());
 std::unique_ptr<btDbvtBroadphase> broad=std::make_unique<btDbvtBroadphase>();
 std::unique_ptr<btSequentialImpulseConstraintSolver> solver=std::make_unique<btSequentialImpulseConstraintSolver>();
 std::unique_ptr<btDiscreteDynamicsWorld> world=std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(),broad.get(),solver.get(),config.get());
 std::unordered_map<EntityId,Body> bodies;
 std::unordered_map<EntityId,std::unique_ptr<btTypedConstraint>> joints;
 std::unordered_map<EntityId,RhizoPhysicsAnimationInfo> animation;
 Impl(){world->setGravity({0,-9.81f,0});world->getSolverInfo().m_numIterations=10;}
 void dropJoint(EntityId id){auto i=joints.find(id);if(i!=joints.end()){world->removeConstraint(i->second.get());joints.erase(i);}}
 void dropBody(EntityId id){for(auto i=joints.begin();i!=joints.end();){auto a=i->second->getRigidBodyA().getUserIndex(),b=i->second->getRigidBodyB().getUserIndex();if(a==static_cast<int>(id)||b==static_cast<int>(id)){world->removeConstraint(i->second.get());i=joints.erase(i);}else ++i;}auto i=bodies.find(id);if(i!=bodies.end()){world->removeRigidBody(i->second.rigid.get());bodies.erase(i);}}
};
PhysicsWorld::PhysicsWorld():impl_(std::make_unique<Impl>()){}
PhysicsWorld::~PhysicsWorld()=default;
bool PhysicsWorld::create_body(Scene& s,EntityId id,const RhizoRigidBodyInfo& d,std::string& error){
 if(!s.get(id)||d.struct_size<sizeof(d)||d.version!=1||d.body_type>RHIZO_BODY_KINEMATIC||d.shape_type>RHIZO_SHAPE_MESH||!valid3(d.dimensions)||d.dimensions[0]<=0||d.dimensions[1]<=0||d.dimensions[2]<=0||!std::isfinite(d.mass)||d.mass<0||!std::isfinite(d.density)||d.density<=0||!std::isfinite(d.friction)||d.friction<0||!std::isfinite(d.restitution)||d.restitution<0||d.restitution>1||!std::isfinite(d.linear_damping)||d.linear_damping<0||!std::isfinite(d.angular_damping)||d.angular_damping<0){error="Invalid rigid-body descriptor";return false;}
 if(d.shape_type==RHIZO_SHAPE_MESH&&(d.mesh_index>=s.meshes.size()||(d.body_type==RHIZO_BODY_DYNAMIC&&d.mass<=0))){error="Mesh collision requires a valid mesh; dynamic mesh bodies require explicit mass";return false;}
 impl_->dropBody(id);Impl::Body b;b.desc=d;btVector3 dims(d.dimensions[0],d.dimensions[1],d.dimensions[2]);
 if(d.shape_type==RHIZO_SHAPE_BOX)b.shape=std::make_unique<btBoxShape>(dims);
 else if(d.shape_type==RHIZO_SHAPE_SPHERE)b.shape=std::make_unique<btSphereShape>(d.dimensions[0]);
 else if(d.shape_type==RHIZO_SHAPE_CAPSULE)b.shape=std::make_unique<btCapsuleShape>(d.dimensions[0],2*d.dimensions[1]);
 else {const auto& m=s.meshes[d.mesh_index];if(d.body_type==RHIZO_BODY_STATIC){b.triangles=std::make_unique<btTriangleMesh>();for(size_t n=0;n+2<m.indices.size();n+=3){auto ia=m.indices[n],ib=m.indices[n+1],ic=m.indices[n+2];if(ia>=m.vertices.size()||ib>=m.vertices.size()||ic>=m.vertices.size())continue;auto a=m.vertices[ia].position,c=m.vertices[ib].position,e=m.vertices[ic].position;b.triangles->addTriangle({a.x,a.y,a.z},{c.x,c.y,c.z},{e.x,e.y,e.z},true);}b.shape=std::make_unique<btBvhTriangleMeshShape>(b.triangles.get(),true,true);}else{auto hull=std::make_unique<btConvexHullShape>();for(const auto& v:m.vertices)hull->addPoint({v.position.x,v.position.y,v.position.z},false);hull->recalcLocalAabb();b.shape=std::move(hull);}}
 float mass=d.body_type==RHIZO_BODY_DYNAMIC?(d.mass>0?d.mass:d.density*volume(d)):0;btVector3 inertia(0,0,0);if(mass>0)b.shape->calculateLocalInertia(mass,inertia);b.motion=std::make_unique<btDefaultMotionState>(toBt(s.get(id)->local));btRigidBody::btRigidBodyConstructionInfo ci(mass,b.motion.get(),b.shape.get(),inertia);ci.m_friction=d.friction;ci.m_restitution=d.restitution;ci.m_linearDamping=d.linear_damping;ci.m_angularDamping=d.angular_damping;b.rigid=std::make_unique<btRigidBody>(ci);b.rigid->setUserIndex(static_cast<int>(id));if(d.body_type==RHIZO_BODY_KINEMATIC){b.rigid->setCollisionFlags(b.rigid->getCollisionFlags()|btCollisionObject::CF_KINEMATIC_OBJECT);b.rigid->setActivationState(DISABLE_DEACTIVATION);}impl_->world->addRigidBody(b.rigid.get(),static_cast<short>(d.collision_group?d.collision_group:1),static_cast<short>(d.collision_mask?d.collision_mask:-1));impl_->bodies.emplace(id,std::move(b));auto& bytes=s.components.emplace<CustomComponentSet>(id).values[kBody];bytes.resize(sizeof(d));std::memcpy(bytes.data(),&d,sizeof(d));return true;
}
bool PhysicsWorld::remove_body(Scene& s,EntityId id){impl_->dropBody(id);if(auto* c=s.components.get<CustomComponentSet>(id))c->values.erase(kBody);return true;}
bool PhysicsWorld::create_joint(Scene& s,EntityId id,const RhizoPhysicsJointInfo& d,std::string& error){
 if(d.struct_size<sizeof(d)||d.version!=1||d.joint_type>RHIZO_JOINT_DISTANCE||id==d.connected_entity||!impl_->bodies.contains(id)||!impl_->bodies.contains(d.connected_entity)||!valid3(d.pivot_a)||!valid3(d.pivot_b)||!valid3(d.axis_a)||!valid3(d.axis_b)||!std::isfinite(d.lower_limit)||!std::isfinite(d.upper_limit)||d.lower_limit>d.upper_limit||!std::isfinite(d.stiffness)||d.stiffness<0||!std::isfinite(d.damping)||d.damping<0){error="Invalid joint descriptor or missing rigid body";return false;}
 impl_->dropJoint(id);auto& a=*impl_->bodies.at(id).rigid;auto& b=*impl_->bodies.at(d.connected_entity).rigid;btVector3 pa=v3(d.pivot_a),pb=v3(d.pivot_b),aa=v3(d.axis_a).normalized(),ab=v3(d.axis_b).normalized();std::unique_ptr<btTypedConstraint> j;
 if(d.joint_type==RHIZO_JOINT_FIXED){btTransform fa,fb;fa.setIdentity();fb.setIdentity();fa.setOrigin(pa);fb.setOrigin(pb);j=std::make_unique<btFixedConstraint>(a,b,fa,fb);}
 else if(d.joint_type==RHIZO_JOINT_HINGE)j=std::make_unique<btHingeConstraint>(a,b,pa,pb,aa,ab,true);
 else{auto x=std::make_unique<btGeneric6DofSpring2Constraint>(a,b,btTransform(btQuaternion::getIdentity(),pa),btTransform(btQuaternion::getIdentity(),pb));x->setLinearLowerLimit({d.lower_limit,0,0});x->setLinearUpperLimit({d.upper_limit,0,0});x->setAngularLowerLimit({0,0,0});x->setAngularUpperLimit({0,0,0});if(d.joint_type==RHIZO_JOINT_SPRING)for(int k=0;k<3;k++){x->enableSpring(k,true);x->setStiffness(k,d.stiffness);x->setDamping(k,d.damping);}j=std::move(x);}
 impl_->world->addConstraint(j.get(),d.disable_collision!=0);impl_->joints.emplace(id,std::move(j));auto& bytes=s.components.emplace<CustomComponentSet>(id).values[kJoint];bytes.resize(sizeof(d));std::memcpy(bytes.data(),&d,sizeof(d));return true;
}
bool PhysicsWorld::remove_joint(Scene& s,EntityId id){impl_->dropJoint(id);if(auto* c=s.components.get<CustomComponentSet>(id))c->values.erase(kJoint);return true;}
bool PhysicsWorld::set_animation_mode(Scene& s,EntityId id,const RhizoPhysicsAnimationInfo& d){
 if(!s.get(id)||d.struct_size<sizeof(d)||d.mode>RHIZO_PHYSICS_ANIMATION_BLEND||!std::isfinite(d.blend)||d.blend<0||d.blend>1)return false;
 impl_->animation[id]=d;
 auto body=impl_->bodies.find(id);
 if(body!=impl_->bodies.end()){
  auto& rb=*body->second.rigid;
  if(d.mode==RHIZO_PHYSICS_ANIMATION_DRIVES){rb.setCollisionFlags(rb.getCollisionFlags()|btCollisionObject::CF_KINEMATIC_OBJECT);rb.setMassProps(0,btVector3(0,0,0));rb.setActivationState(DISABLE_DEACTIVATION);}
  else if(body->second.desc.body_type==RHIZO_BODY_DYNAMIC){rb.setCollisionFlags(rb.getCollisionFlags()&~btCollisionObject::CF_KINEMATIC_OBJECT);float mass=body->second.desc.mass>0?body->second.desc.mass:body->second.desc.density*volume(body->second.desc);btVector3 inertia(0,0,0);body->second.shape->calculateLocalInertia(mass,inertia);rb.setMassProps(mass,inertia);rb.updateInertiaTensor();rb.activate(true);}
 }
 auto& bytes=s.components.emplace<CustomComponentSet>(id).values[kAnim];bytes.resize(sizeof(d));std::memcpy(bytes.data(),&d,sizeof(d));return true;
}
bool PhysicsWorld::set_velocity(EntityId id,const float* l,const float* a){auto i=impl_->bodies.find(id);if(i==impl_->bodies.end()||!valid3(l)||!valid3(a))return false;i->second.rigid->setLinearVelocity(v3(l));i->second.rigid->setAngularVelocity(v3(a));i->second.rigid->activate(true);return true;}
bool PhysicsWorld::apply_force(EntityId id,const float* f,bool impulse){auto i=impl_->bodies.find(id);if(i==impl_->bodies.end()||!valid3(f)||i->second.desc.body_type!=RHIZO_BODY_DYNAMIC)return false;if(impulse)i->second.rigid->applyCentralImpulse(v3(f));else i->second.rigid->applyCentralForce(v3(f));i->second.rigid->activate(true);return true;}
bool PhysicsWorld::raycast(const float* a,const float* b,RhizoRaycastHit& out)const{if(!valid3(a)||!valid3(b))return false;btCollisionWorld::ClosestRayResultCallback cb(v3(a),v3(b));impl_->world->rayTest(v3(a),v3(b),cb);if(!cb.hasHit())return false;out.entity_id=static_cast<uint32_t>(cb.m_collisionObject->getUserIndex());out.fraction=static_cast<float>(cb.m_closestHitFraction);for(int k=0;k<3;k++){out.point[k]=cb.m_hitPointWorld[k];out.normal[k]=cb.m_hitNormalWorld[k];}return true;}
bool PhysicsWorld::step(Scene& s,double dt,std::string& error){if(!std::isfinite(dt)||dt<0||dt>0.25){error="Physics delta must be in [0, 0.25] seconds";return false;}for(auto i=impl_->bodies.begin();i!=impl_->bodies.end();){if(!s.get(i->first)){auto id=i->first;++i;impl_->dropBody(id);}else ++i;}for(auto& [id,b]:impl_->bodies){auto a=impl_->animation.find(id);bool animated=a!=impl_->animation.end()&&a->second.mode==RHIZO_PHYSICS_ANIMATION_DRIVES;if(b.desc.body_type==RHIZO_BODY_KINEMATIC||animated){auto* n=s.get(id);if(n){auto t=toBt(n->local);b.rigid->setWorldTransform(t);b.rigid->getMotionState()->setWorldTransform(t);b.rigid->activate(true);}}}if(dt>0)impl_->world->stepSimulation(static_cast<btScalar>(dt),4,1.0f/60.0f);for(auto& [id,b]:impl_->bodies){if(b.desc.body_type!=RHIZO_BODY_DYNAMIC)continue;auto* n=s.get(id);if(!n)continue;auto a=impl_->animation.find(id);if(a!=impl_->animation.end()&&a->second.mode==RHIZO_PHYSICS_ANIMATION_DRIVES)continue;auto t=fromBt(b.rigid->getWorldTransform());if(a!=impl_->animation.end()&&a->second.mode==RHIZO_PHYSICS_ANIMATION_BLEND){float w=a->second.blend;n->local.position=n->local.position*(1-w)+t.position*w;n->local.rotation=slerp(n->local.rotation,t.rotation,w);}else{n->local.position=t.position;n->local.rotation=t.rotation;}}return true;}
void PhysicsWorld::clear(){for(auto& [id,j]:impl_->joints){(void)id;impl_->world->removeConstraint(j.get());}impl_->joints.clear();for(auto& [id,b]:impl_->bodies){(void)id;impl_->world->removeRigidBody(b.rigid.get());}impl_->bodies.clear();impl_->animation.clear();}
bool PhysicsWorld::rebuild(Scene& s,std::string& error){
 clear();
 for(const auto& node:s.nodes()){
  const auto* set=s.components.get<CustomComponentSet>(node.id);if(!set)continue;
  auto body=set->values.find(kBody);if(body!=set->values.end()&&body->second.size()==sizeof(RhizoRigidBodyInfo)){RhizoRigidBodyInfo d{};std::memcpy(&d,body->second.data(),sizeof(d));if(!create_body(s,node.id,d,error))return false;}
 }
 for(const auto& node:s.nodes()){
  const auto* set=s.components.get<CustomComponentSet>(node.id);if(!set)continue;
  auto anim=set->values.find(kAnim);if(anim!=set->values.end()&&anim->second.size()==sizeof(RhizoPhysicsAnimationInfo)){RhizoPhysicsAnimationInfo d{};std::memcpy(&d,anim->second.data(),sizeof(d));if(!set_animation_mode(s,node.id,d)){error="Invalid serialized physics-animation mode";return false;}}
 }
 for(const auto& node:s.nodes()){
  const auto* set=s.components.get<CustomComponentSet>(node.id);if(!set)continue;
  auto joint=set->values.find(kJoint);if(joint!=set->values.end()&&joint->second.size()==sizeof(RhizoPhysicsJointInfo)){RhizoPhysicsJointInfo d{};std::memcpy(&d,joint->second.data(),sizeof(d));if(!create_joint(s,node.id,d,error))return false;}
 }
 return true;
}

}
