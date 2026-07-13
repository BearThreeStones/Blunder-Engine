#include "runtime/core/object/object_db.h"

#include <cstddef>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/object/entity_store.h"

namespace Blunder {
namespace {

struct Slot {
  eastl::unique_ptr<Object> object;
  uint32_t generation{1};
  bool occupied{false};
};

eastl::vector<Slot>& slots() {
  static eastl::vector<Slot> s_slots;
  return s_slots;
}

IEntityStore*& entityStore() {
  static IEntityStore* s_store = nullptr;
  return s_store;
}

}  // namespace

void ObjectDB::clear() {
  slots().clear();
  entityStore() = nullptr;
}

ObjectId ObjectDB::create() {
  eastl::vector<Slot>& s = slots();
  uint32_t index = 0;
  for (; index < s.size(); ++index) {
    if (!s[index].occupied) {
      break;
    }
  }
  if (index == s.size()) {
    s.push_back(Slot{});
  }

  Slot& slot = s[index];
  slot.object = eastl::make_unique<Object>();
  slot.occupied = true;
  if (slot.generation == 0) {
    slot.generation = 1;
  }
  const ObjectId id = makeObjectId(index, slot.generation);
  slot.object->m_id = id;
  return id;
}

Object* ObjectDB::get(ObjectId id) {
  if (!isValid(id)) {
    return nullptr;
  }
  const uint32_t index = objectIdIndex(id);
  eastl::vector<Slot>& s = slots();
  if (index >= s.size()) {
    return nullptr;
  }
  Slot& slot = s[index];
  if (!slot.occupied || slot.generation != objectIdGeneration(id) ||
      slot.object == nullptr) {
    return nullptr;
  }
  return slot.object.get();
}

void ObjectDB::destroy(ObjectId id) {
  Object* object = get(id);
  if (object == nullptr) {
    return;
  }

  if (isValid(object->m_parent_id)) {
    Object* parent = get(object->m_parent_id);
    if (parent != nullptr) {
      auto& children = parent->m_children;
      for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == id) {
          children.erase(children.begin() + static_cast<ptrdiff_t>(i));
          break;
        }
      }
    }
    object->m_parent_id = k_invalid_object_id;
  }

  eastl::vector<ObjectId> children = object->m_children;
  object->m_children.clear();
  for (ObjectId child_id : children) {
    destroy(child_id);
  }

  object->m_entity_id = k_invalid_entity_id;

  const uint32_t index = objectIdIndex(id);
  Slot& slot = slots()[index];
  slot.object.reset();
  slot.occupied = false;
  ++slot.generation;
  if (slot.generation == 0) {
    slot.generation = 1;
  }
}

void ObjectDB::setEntityStore(IEntityStore* store) { entityStore() = store; }

IEntityStore* ObjectDB::getEntityStore() { return entityStore(); }

}  // namespace Blunder
