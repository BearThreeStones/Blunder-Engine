#include "runtime/core/object/object.h"

#include <cstddef>
#include <cstring>

#include "runtime/core/object/entity_store.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

namespace {

void object_apply_skeleton_modifiers(Skeleton& skeleton, void* userdata) {
  if (userdata == nullptr) {
    return;
  }
  static_cast<Object*>(userdata)->applySkeletonModifiers(skeleton);
}

}  // namespace

void Object::setParent(Object* parent) {
  if (isValid(m_parent_id)) {
    Object* old_parent = ObjectDB::get(m_parent_id);
    if (old_parent != nullptr) {
      for (size_t i = 0; i < old_parent->m_children.size(); ++i) {
        if (old_parent->m_children[i] == m_id) {
          old_parent->m_children.erase(old_parent->m_children.begin() +
                                       static_cast<ptrdiff_t>(i));
          break;
        }
      }
    }
    m_parent_id = k_invalid_object_id;
  }

  if (parent == nullptr) {
    return;
  }

  m_parent_id = parent->m_id;
  parent->m_children.push_back(m_id);
}

ObjectId Object::getChildId(size_t index) const {
  if (index >= m_children.size()) {
    return k_invalid_object_id;
  }
  return m_children[index];
}

Object::BehaviourSlot* Object::findBehaviourSlot(BehaviourId id) {
  for (BehaviourSlot& slot : m_behaviours) {
    if (slot.id == id) {
      return &slot;
    }
  }
  return nullptr;
}

const Object::BehaviourSlot* Object::findBehaviourSlot(BehaviourId id) const {
  for (const BehaviourSlot& slot : m_behaviours) {
    if (slot.id == id) {
      return &slot;
    }
  }
  return nullptr;
}

BehaviourId Object::addBehaviour(eastl::string type_name) {
  BehaviourSlot slot;
  slot.id = m_next_behaviour_id++;
  slot.type_name = eastl::move(type_name);
  slot.script_peer = nullptr;
  slot.ready_invoked = false;
  m_behaviours.push_back(eastl::move(slot));
  return m_behaviours.back().id;
}

bool Object::restoreBehaviour(BehaviourId id, eastl::string type_name) {
  if (!isValidBehaviourId(id) || findBehaviourSlot(id) != nullptr) {
    return false;
  }
  BehaviourSlot slot;
  slot.id = id;
  slot.type_name = eastl::move(type_name);
  slot.script_peer = nullptr;
  slot.ready_invoked = false;
  m_behaviours.push_back(eastl::move(slot));
  if (id >= m_next_behaviour_id) {
    m_next_behaviour_id = id + 1;
  }
  return true;
}

bool Object::removeBehaviour(BehaviourId id) {
  for (size_t i = 0; i < m_behaviours.size(); ++i) {
    if (m_behaviours[i].id == id) {
      m_behaviours.erase(m_behaviours.begin() + static_cast<ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

BehaviourId Object::getBehaviourIdAt(size_t index) const {
  if (index >= m_behaviours.size()) {
    return k_invalid_behaviour_id;
  }
  return m_behaviours[index].id;
}

const char* Object::getBehaviourTypeName(BehaviourId id) const {
  const BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return nullptr;
  }
  return slot->type_name.c_str();
}

void Object::setBehaviourScriptPeer(BehaviourId id, void* peer) {
  BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot != nullptr) {
    slot->script_peer = peer;
    slot->ready_invoked = false;
  }
}

void* Object::getBehaviourScriptPeer(BehaviourId id) const {
  const BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return nullptr;
  }
  return slot->script_peer;
}

bool Object::isBehaviourReadyInvoked(BehaviourId id) const {
  const BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return false;
  }
  return slot->ready_invoked;
}

void Object::markBehaviourReadyInvoked(BehaviourId id) {
  BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot != nullptr) {
    slot->ready_invoked = true;
  }
}

const eastl::vector<SceneBehaviourProperty>* Object::getBehaviourProperties(
    BehaviourId id) const {
  const BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return nullptr;
  }
  return &slot->properties;
}

bool Object::setBehaviourProperties(
    BehaviourId id, eastl::vector<SceneBehaviourProperty> properties) {
  BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return false;
  }
  slot->properties = eastl::move(properties);
  return true;
}

bool Object::moveBehaviour(size_t from_index, size_t to_index) {
  const size_t count = m_behaviours.size();
  if (count == 0 || from_index >= count) {
    return false;
  }
  if (to_index > count) {
    to_index = count;
  }
  if (from_index == to_index) {
    return true;
  }

  BehaviourSlot slot = eastl::move(m_behaviours[from_index]);
  m_behaviours.erase(m_behaviours.begin() + static_cast<ptrdiff_t>(from_index));
  if (from_index < to_index) {
    --to_index;
  }
  if (to_index > m_behaviours.size()) {
    to_index = m_behaviours.size();
  }
  m_behaviours.insert(m_behaviours.begin() + static_cast<ptrdiff_t>(to_index),
                      eastl::move(slot));
  return true;
}

void* Object::getScriptPeer() const {
  if (m_behaviours.empty()) {
    return nullptr;
  }
  return m_behaviours[0].script_peer;
}

void Object::setScriptPeer(void* peer) {
  if (m_behaviours.empty()) {
    return;
  }
  m_behaviours[0].script_peer = peer;
  m_behaviours[0].ready_invoked = false;
}

void Object::clearScriptPeer() {
  if (m_behaviours.empty()) {
    return;
  }
  m_behaviours[0].script_peer = nullptr;
  m_behaviours[0].ready_invoked = false;
}

Vec3 Object::getPosition() const {
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    if (store != nullptr &&
        store->getTransform(m_entity_id, position, rotation, scale)) {
      return position;
    }
  }
  return m_local_position;
}

void Object::setPosition(const Vec3& position) {
  m_local_position = position;
  m_has_local_trs = true;
  materializeEntityIfNeeded();
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    if (store != nullptr) {
      store->setTransform(m_entity_id, m_local_position, m_local_rotation,
                          m_local_scale);
      store->markTransformsDirty();
    }
  }
}

Quat Object::getRotation() const {
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    if (store != nullptr &&
        store->getTransform(m_entity_id, position, rotation, scale)) {
      return rotation;
    }
  }
  return m_local_rotation;
}

void Object::setRotation(const Quat& rotation) {
  m_local_rotation = rotation;
  m_has_local_trs = true;
  materializeEntityIfNeeded();
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    if (store != nullptr) {
      store->setTransform(m_entity_id, m_local_position, m_local_rotation,
                          m_local_scale);
      store->markTransformsDirty();
    }
  }
}

Vec3 Object::getScale() const {
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    if (store != nullptr &&
        store->getTransform(m_entity_id, position, rotation, scale)) {
      return scale;
    }
  }
  return m_local_scale;
}

void Object::setScale(const Vec3& scale) {
  m_local_scale = scale;
  m_has_local_trs = true;
  materializeEntityIfNeeded();
  if (hasEntity()) {
    IEntityStore* store = ObjectDB::getEntityStore();
    if (store != nullptr) {
      store->setTransform(m_entity_id, m_local_position, m_local_rotation,
                          m_local_scale);
      store->markTransformsDirty();
    }
  }
}

void Object::materializeEntityIfNeeded() {
  if (hasEntity()) {
    return;
  }
  IEntityStore* store = ObjectDB::getEntityStore();
  if (store == nullptr) {
    return;
  }
  EntityId parent_entity = k_invalid_entity_id;
  if (isValid(m_parent_id)) {
    Object* parent = ObjectDB::get(m_parent_id);
    if (parent != nullptr && parent->hasEntity()) {
      parent_entity = parent->getEntityId();
    }
  }
  m_entity_id = store->createEntity(m_name, m_local_position, m_local_rotation,
                                    m_local_scale, parent_entity);
}

Skeleton* Object::ensureSkeleton() {
  if (m_skeleton == nullptr) {
    m_skeleton = eastl::make_unique<Skeleton>();
  }
  updateAnimationSamplingBinding();
  return m_skeleton.get();
}

void Object::clearSkeleton() {
  m_skeleton.reset();
  updateAnimationSamplingBinding();
}

AnimationPlayer* Object::ensureAnimationPlayer() {
  if (m_animation_player == nullptr) {
    m_animation_player = eastl::make_unique<AnimationPlayer>();
  }
  updateAnimationSamplingBinding();
  return m_animation_player.get();
}

void Object::clearAnimationPlayer() {
  m_animation_player.reset();
  updateAnimationSamplingBinding();
}

AnimationTree* Object::ensureAnimationTree() {
  if (m_animation_tree == nullptr) {
    m_animation_tree = eastl::make_unique<AnimationTree>();
  }
  updateAnimationSamplingBinding();
  return m_animation_tree.get();
}

void Object::clearAnimationTree() {
  m_animation_tree.reset();
  updateAnimationSamplingBinding();
}

SkeletonModifier* Object::getSkeletonModifierAt(size_t index) {
  if (index >= m_skeleton_modifiers.size()) {
    return nullptr;
  }
  return m_skeleton_modifiers[index].get();
}

const SkeletonModifier* Object::getSkeletonModifierAt(size_t index) const {
  if (index >= m_skeleton_modifiers.size()) {
    return nullptr;
  }
  return m_skeleton_modifiers[index].get();
}

SkeletonModifier* Object::addSkeletonModifier() {
  return addSkeletonModifier(eastl::make_unique<SkeletonModifier>());
}

SkeletonModifier* Object::addSkeletonModifier(
    eastl::unique_ptr<SkeletonModifier> modifier) {
  if (modifier == nullptr) {
    return nullptr;
  }
  m_skeleton_modifiers.push_back(eastl::move(modifier));
  updateAnimationSamplingBinding();
  return m_skeleton_modifiers.back().get();
}

SkeletonLookAtModifier* Object::addSkeletonLookAtModifier() {
  return static_cast<SkeletonLookAtModifier*>(
      addSkeletonModifier(eastl::make_unique<SkeletonLookAtModifier>()));
}

SkeletonPaperMouthModifier* Object::addSkeletonPaperMouthModifier() {
  return static_cast<SkeletonPaperMouthModifier*>(
      addSkeletonModifier(eastl::make_unique<SkeletonPaperMouthModifier>()));
}

SkeletonAttachModifier* Object::addSkeletonAttachModifier() {
  return static_cast<SkeletonAttachModifier*>(
      addSkeletonModifier(eastl::make_unique<SkeletonAttachModifier>()));
}

bool Object::moveSkeletonModifier(size_t from_index, size_t to_index) {
  const size_t count = m_skeleton_modifiers.size();
  if (count == 0 || from_index >= count) {
    return false;
  }
  if (to_index > count) {
    to_index = count;
  }
  if (from_index == to_index) {
    return true;
  }
  eastl::unique_ptr<SkeletonModifier> moving =
      eastl::move(m_skeleton_modifiers[from_index]);
  m_skeleton_modifiers.erase(m_skeleton_modifiers.begin() +
                             static_cast<ptrdiff_t>(from_index));
  if (from_index < to_index) {
    --to_index;
  }
  if (to_index > m_skeleton_modifiers.size()) {
    to_index = m_skeleton_modifiers.size();
  }
  m_skeleton_modifiers.insert(
      m_skeleton_modifiers.begin() + static_cast<ptrdiff_t>(to_index),
      eastl::move(moving));
  updateAnimationSamplingBinding();
  return true;
}

bool Object::removeSkeletonModifierAt(const size_t index) {
  if (index >= m_skeleton_modifiers.size()) {
    return false;
  }
  m_skeleton_modifiers.erase(m_skeleton_modifiers.begin() +
                             static_cast<ptrdiff_t>(index));
  updateAnimationSamplingBinding();
  return true;
}

bool Object::insertSkeletonModifierAt(
    const size_t index, eastl::unique_ptr<SkeletonModifier> modifier) {
  if (modifier == nullptr || index > m_skeleton_modifiers.size()) {
    return false;
  }
  m_skeleton_modifiers.insert(
      m_skeleton_modifiers.begin() + static_cast<ptrdiff_t>(index),
      eastl::move(modifier));
  updateAnimationSamplingBinding();
  return true;
}

void Object::applySkeletonModifiers(Skeleton& skeleton) {
  const Mat4 host_world = computeWorldMatrix();
  for (const eastl::unique_ptr<SkeletonModifier>& modifier : m_skeleton_modifiers) {
    if (modifier == nullptr) {
      continue;
    }
    if (std::strcmp(modifier->getTypeName(), "SkeletonLookAtModifier") == 0) {
      static_cast<SkeletonLookAtModifier*>(modifier.get())
          ->setHostWorldMatrix(host_world);
    }
    modifier->apply(skeleton);
  }
}

Mat4 Object::computeWorldMatrix() const {
  const Mat4 translation = glm::translate(Mat4(1.0f), getPosition());
  const Mat4 rotation = glm::mat4_cast(getRotation());
  const Mat4 scale = glm::scale(Mat4(1.0f), getScale());
  Mat4 local = translation * rotation * scale;

  ObjectId parent_id = m_parent_id;
  while (isValid(parent_id)) {
    const Object* parent = ObjectDB::get(parent_id);
    if (parent == nullptr) {
      break;
    }
    const Mat4 parent_translation =
        glm::translate(Mat4(1.0f), parent->getPosition());
    const Mat4 parent_rotation = glm::mat4_cast(parent->getRotation());
    const Mat4 parent_scale = glm::scale(Mat4(1.0f), parent->getScale());
    const Mat4 parent_local = parent_translation * parent_rotation * parent_scale;
    local = parent_local * local;
    parent_id = parent->getParentId();
  }
  return local;
}

void Object::updateAnimationSamplingBinding() {
  if (m_animation_player != nullptr) {
    m_animation_player->bindOwnerObject(m_id);
    m_animation_player->bindSamplingSkeleton(m_skeleton.get());
    if (m_skeleton != nullptr) {
      m_animation_player->bindSkeletonModifierChain(&object_apply_skeleton_modifiers,
                                                    this);
    } else {
      m_animation_player->bindSkeletonModifierChain(nullptr, nullptr);
    }
  }
  if (m_animation_tree != nullptr) {
    m_animation_tree->bindAnimationPlayer(m_animation_player.get());
    m_animation_tree->bindSamplingSkeleton(m_skeleton.get());
    if (m_skeleton != nullptr) {
      m_animation_tree->bindSkeletonModifierChain(&object_apply_skeleton_modifiers,
                                                  this);
    } else {
      m_animation_tree->bindSkeletonModifierChain(nullptr, nullptr);
    }
    if (m_animation_player != nullptr) {
      m_animation_player->setTreeBlocksSampling(m_animation_tree->isActive());
    }
  } else if (m_animation_player != nullptr) {
    m_animation_player->bindAnimationTree(nullptr);
    m_animation_player->setTreeBlocksSampling(false);
  }
}

void Object::syncLocalTransformFromStore() {
  if (!hasEntity()) {
    return;
  }
  IEntityStore* store = ObjectDB::getEntityStore();
  if (store == nullptr) {
    return;
  }
  Vec3 position;
  Quat rotation;
  Vec3 scale;
  if (!store->getTransform(m_entity_id, position, rotation, scale)) {
    return;
  }
  m_local_position = position;
  m_local_rotation = rotation;
  m_local_scale = scale;
  m_has_local_trs = true;
}

}  // namespace Blunder
