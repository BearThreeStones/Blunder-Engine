#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

size_t findBehaviourIndex(Blunder::Object* object, Blunder::BehaviourId id) {
  for (size_t i = 0; i < object->getBehaviourCount(); ++i) {
    if (object->getBehaviourIdAt(i) == id) {
      return i;
    }
  }
  return static_cast<size_t>(-1);
}

}  // namespace

int main() {
  using namespace Blunder;

  // Add behaviour command round-trip
  {
    SceneInstance scene;
    const EntityId entity_id =
        scene.createEntity("E", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const eastl::string clr_type = "Game.Motor";

    Object* object = scene.ensureBoundObject(entity_id);
    expect_true("ensure bound", object != nullptr);
    const BehaviourId created_id = object->addBehaviour(clr_type);
    expect_true("valid id", isValidBehaviourId(created_id));
    expect_true("count after add", object->getBehaviourCount() == 1);

    DocumentHistory history;
    history.push(makeAddBehaviourCommand(&scene, entity_id, clr_type, created_id,
                                         SelectionSnapshot{entity_id},
                                         SelectionSnapshot{entity_id}));

    expect_true("undo add", history.undo());
    expect_true("removed after undo", object->getBehaviourCount() == 0);
    expect_true("redo add", history.redo());
    expect_true("restored count", object->getBehaviourCount() == 1);
    expect_true("restored id", object->getBehaviourIdAt(0) == created_id);
    expect_true("restored type",
                eastl::string(object->getBehaviourTypeName(created_id)) ==
                    clr_type);
  }

  // Remove behaviour command round-trip
  {
    SceneInstance scene;
    const EntityId entity_id =
        scene.createEntity("R", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    Object* object = scene.ensureBoundObject(entity_id);
    expect_true("ensure bound remove", object != nullptr);

    const BehaviourId id_a = object->addBehaviour("Game.A");
    const BehaviourId id_b = object->addBehaviour("Game.B");
    eastl::vector<SceneBehaviourProperty> bag;
    SceneBehaviourProperty prop;
    prop.key = "Speed";
    prop.value = Variant(2.0f);
    bag.push_back(prop);
    expect_true("set bag remove", object->setBehaviourProperties(id_a, bag));

    const size_t index_a = findBehaviourIndex(object, id_a);
    const eastl::string type_a = object->getBehaviourTypeName(id_a);
    eastl::vector<SceneBehaviourProperty> snapshot = bag;
    expect_true("remove applied", object->removeBehaviour(id_a));
    expect_true("one left", object->getBehaviourCount() == 1);
    expect_true("b remains", object->getBehaviourIdAt(0) == id_b);

    DocumentHistory history;
    history.push(makeRemoveBehaviourCommand(
        &scene, entity_id, id_a, index_a, type_a, eastl::move(snapshot),
        SelectionSnapshot{entity_id}, SelectionSnapshot{entity_id}));

    expect_true("undo remove", history.undo());
    expect_true("two after undo remove", object->getBehaviourCount() == 2);
    expect_true("a restored index",
                object->getBehaviourIdAt(index_a) == id_a);
    const eastl::vector<SceneBehaviourProperty>* restored =
        object->getBehaviourProperties(id_a);
    expect_true("bag restored",
                restored != nullptr && restored->size() == 1 &&
                    (*restored)[0].key == "Speed" &&
                    (*restored)[0].value == Variant(2.0f));
    expect_true("redo remove", history.redo());
    expect_true("one after redo remove", object->getBehaviourCount() == 1);
    expect_true("b only", object->getBehaviourIdAt(0) == id_b);
  }

  // Reorder behaviours command round-trip
  {
    SceneInstance scene;
    const EntityId entity_id =
        scene.createEntity("O", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    Object* object = scene.ensureBoundObject(entity_id);
    const BehaviourId id_first = object->addBehaviour("Game.First");
    const BehaviourId id_second = object->addBehaviour("Game.Second");
    expect_true("reorder move", object->moveBehaviour(0, 2));
    expect_true("second first", object->getBehaviourIdAt(0) == id_second);
    expect_true("first second", object->getBehaviourIdAt(1) == id_first);

    DocumentHistory history;
    history.push(makeReorderBehavioursCommand(&scene, entity_id, 0, 2,
                                              SelectionSnapshot{entity_id},
                                              SelectionSnapshot{entity_id}));

    expect_true("undo reorder", history.undo());
    expect_true("first back", object->getBehaviourIdAt(0) == id_first);
    expect_true("second back", object->getBehaviourIdAt(1) == id_second);
    expect_true("redo reorder", history.redo());
    expect_true("second front", object->getBehaviourIdAt(0) == id_second);
    expect_true("first back", object->getBehaviourIdAt(1) == id_first);
  }

  // Set behaviour property command round-trip
  {
    SceneInstance scene;
    const EntityId entity_id =
        scene.createEntity("P", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    Object* object = scene.ensureBoundObject(entity_id);
    const BehaviourId behaviour_id = object->addBehaviour("Game.Probe");
    const Variant before(1.0f);
    const Variant after(3.5f);

    eastl::vector<SceneBehaviourProperty> bag;
    SceneBehaviourProperty prop;
    prop.key = "Speed";
    prop.value = before;
    bag.push_back(prop);
    object->setBehaviourProperties(behaviour_id, bag);
    prop.value = after;
    bag[0] = prop;
    object->setBehaviourProperties(behaviour_id, bag);

    DocumentHistory history;
    history.push(makeSetBehaviourPropertyCommand(
        &scene, entity_id, behaviour_id, "Speed", before, after,
        SelectionSnapshot{entity_id}, SelectionSnapshot{entity_id}));

    const eastl::vector<SceneBehaviourProperty>* props =
        object->getBehaviourProperties(behaviour_id);
    expect_true("props present", props != nullptr && props->size() == 1);
    expect_true("undo set property", history.undo());
    props = object->getBehaviourProperties(behaviour_id);
    expect_true("before restored",
                props != nullptr && props->size() == 1 &&
                    props->at(0).value == before);
    expect_true("redo set property", history.redo());
    props = object->getBehaviourProperties(behaviour_id);
    expect_true("after restored",
                props != nullptr && props->size() == 1 &&
                    props->at(0).value == after);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("inspector_behaviour_commands_test: OK\n");
  return 0;
}
