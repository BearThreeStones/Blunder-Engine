#include "runtime/core/object/entity_store.h"
#include "runtime/core/object/object_db.h"

#include <cstdio>

#include "EASTL/vector.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

class FakeEntityStore final : public Blunder::IEntityStore {
 public:
  Blunder::EntityId createEntity(eastl::string name,
                                 const Blunder::Vec3& position,
                                 const Blunder::Quat& rotation,
                                 const Blunder::Vec3& scale,
                                 Blunder::EntityId parent_id) override {
    Entry entry;
    entry.name = eastl::move(name);
    entry.position = position;
    entry.rotation = rotation;
    entry.scale = scale;
    entry.parent_id = parent_id;
    m_entries.push_back(entry);
    return static_cast<Blunder::EntityId>(m_entries.size());
  }

  bool getTransform(Blunder::EntityId id, Blunder::Vec3& out_position,
                    Blunder::Quat& out_rotation,
                    Blunder::Vec3& out_scale) const override {
    if (!Blunder::isValid(id) ||
        id > static_cast<Blunder::EntityId>(m_entries.size())) {
      return false;
    }
    const Entry& entry = m_entries[id - 1u];
    out_position = entry.position;
    out_rotation = entry.rotation;
    out_scale = entry.scale;
    return true;
  }

  bool setTransform(Blunder::EntityId id, const Blunder::Vec3& position,
                    const Blunder::Quat& rotation,
                    const Blunder::Vec3& scale) override {
    if (!Blunder::isValid(id) ||
        id > static_cast<Blunder::EntityId>(m_entries.size())) {
      return false;
    }
    Entry& entry = m_entries[id - 1u];
    entry.position = position;
    entry.rotation = rotation;
    entry.scale = scale;
    return true;
  }

  void markTransformsDirty() override { ++dirty_count; }

  int dirty_count{0};

 private:
  struct Entry {
    eastl::string name;
    Blunder::Vec3 position{0.0f};
    Blunder::Quat rotation{glm::identity<Blunder::Quat>()};
    Blunder::Vec3 scale{1.0f, 1.0f, 1.0f};
    Blunder::EntityId parent_id{Blunder::k_invalid_entity_id};
  };
  eastl::vector<Entry> m_entries;
};

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();

  expect_true("invalid id is invalid", !isValid(k_invalid_object_id));

  const ObjectId id = ObjectDB::create();
  expect_true("create returns valid id", isValid(id));

  Object* obj = ObjectDB::get(id);
  expect_true("get resolves object", obj != nullptr);
  if (obj != nullptr) {
    expect_true("object id matches", obj->getId() == id);
    obj->setName("Root");
    expect_true("name set", obj->getName() == "Root");
    expect_true("enabled by default", obj->isEnabled());
  }

  const ObjectId child_id = ObjectDB::create();
  Object* child = ObjectDB::get(child_id);
  Object* parent = ObjectDB::get(id);
  expect_true("child exists", child != nullptr && parent != nullptr);
  if (child != nullptr && parent != nullptr) {
    child->setParent(parent);
    expect_true("child parent id", child->getParentId() == id);
    expect_true("parent has child", parent->getChildCount() == 1u);
    expect_true("parent child id", parent->getChildId(0) == child_id);
  }

  if (obj != nullptr) {
    int peer_token = 42;
    expect_true("no peer by default", obj->getScriptPeer() == nullptr);
    obj->setScriptPeer(&peer_token);
    expect_true("peer set", obj->getScriptPeer() == &peer_token);
    obj->clearScriptPeer();
    expect_true("peer cleared", obj->getScriptPeer() == nullptr);
  }

  FakeEntityStore store;
  ObjectDB::setEntityStore(&store);
  Object* lazy = ObjectDB::get(ObjectDB::create());
  expect_true("lazy object", lazy != nullptr);
  if (lazy != nullptr) {
    expect_true("no entity before write", !lazy->hasEntity());
    lazy->setPosition(Vec3(1.0f, 2.0f, 3.0f));
    expect_true("entity after position write", lazy->hasEntity());
    expect_true("position round-trip",
                lazy->getPosition() == Vec3(1.0f, 2.0f, 3.0f));
    expect_true("store marked dirty", store.dirty_count >= 1);
  }

  ObjectDB::destroy(child_id);
  expect_true("destroyed child gone", ObjectDB::get(child_id) == nullptr);
  if (parent != nullptr) {
    expect_true("parent child list cleared", parent->getChildCount() == 0u);
  }

  ObjectDB::destroy(id);
  expect_true("destroyed parent gone", ObjectDB::get(id) == nullptr);

  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
