# Task 1 brief — extracted from plan

Plan: docs/superpowers/plans/2026-07-22-inspector-behaviour-ux.md

### Task 1: Property bag on Object slots + reorder + tests

**Files:**
- Modify: `engine/src/runtime/core/object/object.h`
- Modify: `engine/src/runtime/core/object/object.cpp`
- Modify: `engine/src/runtime/function/scene/scene_instance.cpp` (instantiate + `exportToScene`)
- Create: `engine/src/tests/object_behaviour_bag_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SceneBehaviourProperty`, `Variant` (from `scene.h` / `variant.h`)
- Produces:
  - `BehaviourSlot` gains `eastl::vector<SceneBehaviourProperty> properties`
  - `const eastl::vector<SceneBehaviourProperty>* Object::getBehaviourProperties(BehaviourId) const`
  - `bool Object::setBehaviourProperties(BehaviourId, eastl::vector<SceneBehaviourProperty>)`
  - `bool Object::moveBehaviour(size_t from_index, size_t to_index)` 鈥?moves item; `to_index` is insertion index before move adjust (document in code: clamp; no-op if same)
  - Instantiate copies `decl.properties` after `restoreBehaviour`
  - `exportToScene` copies slot properties into `SceneBehaviourDeclaration`

- [ ] **Step 1: Write the failing test**

```cpp
// engine/src/tests/object_behaviour_bag_test.cpp
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/scene/scene.h"

#include <cstdio>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}
}  // namespace

int main() {
  using namespace Blunder;
  ObjectDB::clear();
  ObjectId oid = ObjectDB::create();
  Object* object = ObjectDB::get(oid);
  expect_true("object", object != nullptr);
  BehaviourId id = object->addBehaviour("Probe.Motor");
  expect_true("id", isValid(id));

  eastl::vector<SceneBehaviourProperty> bag;
  SceneBehaviourProperty p;
  p.key = "Speed";
  p.value = Variant(1.5f);
  bag.push_back(p);
  expect_true("set bag", object->setBehaviourProperties(id, bag));
  const auto* got = object->getBehaviourProperties(id);
  expect_true("get bag", got != nullptr && got->size() == 1 &&
                             (*got)[0].key == "Speed");

  BehaviourId id2 = object->addBehaviour("Probe.Bark");
  expect_true("two", object->getBehaviourCount() == 2);
  expect_true("order0", object->getBehaviourIdAt(0) == id);
  expect_true("move", object->moveBehaviour(0, 2));  // move first to end
  expect_true("order after", object->getBehaviourIdAt(0) == id2 &&
                                 object->getBehaviourIdAt(1) == id);

  if (g_failures) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("object_behaviour_bag_test: OK\n");
  return 0;
}
```

Wire CMake like `behaviour_list_test` (link `engine_runtime` / same PCH pattern).

- [ ] **Step 2: Run test 鈥?expect fail**

```powershell
cmake --build build/vs2026-debug --config Debug --target object_behaviour_bag_test
```

Expected: compile error 鈥?missing `setBehaviourProperties` / `moveBehaviour`.

- [ ] **Step 3: Minimal implementation**

In `object.h` `BehaviourSlot`:

```cpp
    eastl::vector<SceneBehaviourProperty> properties;
```

Include `runtime/function/scene/scene.h` (or extract `SceneBehaviourProperty` to `behaviour_property_bag.h` if include cycle 鈥?prefer extract if needed).

Implement get/set/move; in `scene_instance.cpp` after successful `restoreBehaviour`, assign `decl.properties` onto the slot via set API; in `exportToScene` copy `*getBehaviourProperties` into `decl.properties`.

- [ ] **Step 4: Run test 鈥?expect pass**

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\object_behaviour_bag_test.exe
```

Expected: `object_behaviour_bag_test: OK`

Also extend or run existing `scene_serializer_test` / mount test if bag export covered 鈥?optional follow-up in same task: tiny instantiate鈫抏xport assert in this test via SceneInstance if lightweight.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/object/object.h engine/src/runtime/core/object/object.cpp engine/src/runtime/function/scene/scene_instance.cpp engine/src/tests/object_behaviour_bag_test.cpp engine/src/tests/CMakeLists.txt
git commit -m "feat: store Behaviour property bags on Object slots and support reorder"
```

---
