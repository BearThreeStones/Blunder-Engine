#include "runtime/core/object/object.h"

#include <cstddef>

#include "runtime/core/object/entity_store.h"
#include "runtime/core/object/object_db.h"

namespace Blunder {

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
  m_behaviours.push_back(eastl::move(slot));
  return m_behaviours.back().id;
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
  }
}

void* Object::getBehaviourScriptPeer(BehaviourId id) const {
  const BehaviourSlot* slot = findBehaviourSlot(id);
  if (slot == nullptr) {
    return nullptr;
  }
  return slot->script_peer;
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
}

void Object::clearScriptPeer() {
  if (m_behaviours.empty()) {
    return;
  }
  m_behaviours[0].script_peer = nullptr;
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
