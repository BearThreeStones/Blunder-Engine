# Inspector Behaviour UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let authors Add/Remove/reorder Behaviours and edit bool/number/string fields in the Inspector against scene declarations (no Edit Mode DotNetHost), with Document History and a post-build type catalog.

**Architecture:** Store property bags on `Object` Behaviour slots (instantiate/export round-trip). Refresh `.blunder/behaviour_catalog.json` via a small `net10.0` metadata scanner after `ScriptsBuilder` succeeds. Inspector Slint section drives `EntityId`-addressed Editor Commands. Mount stays Play/Player (`mountSceneBehaviours`).

**Tech Stack:** C++ `engine_runtime`, Slint Inspector, Document History Commands, `ScriptsBuilder`, C# `net10.0` catalog tool, existing scene Behaviour JSON.

## Global Constraints

- Edit Mode authoritative surface: **Behaviour declaration** (not live peer) — ADR 0016
- Mount remains Play/Player (or env-gated debug host) — ADR 0014
- Property editors: **bool / number / string** only
- Add types from **Behaviour type catalog** after successful Scripts build
- Missing types: keep declaration, broken UI, removable; do not block Save/Play
- History: Add / Remove / **drag reorder** / property commit (Enter/focus loss) on **EntityId**
- Catalog-driven forms (member metadata); bag holds values
- Out of scope: Vec3/enum/nested/asset editors; hand-typed CLR as primary Add; AttachBehaviour as Inspector edit API
- Build preset: `vs2026-debug` / `Debug`
- OpenSpec: `openspec/changes/inspector-behaviour-ux/`

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/core/object/object.h/.cpp` | Property bag on `BehaviourSlot`; `moveBehaviour`; bag get/set helpers |
| `engine/src/runtime/function/scene/scene_instance.h/.cpp` | Copy bags on instantiate; export bags; `ensureBoundObject` / `findBoundObject` |
| `engine/src/runtime/function/script/scene_behaviour_mount.cpp` | Prefer Object-slot bag when applying properties (fallback scene decl) |
| `engine/managed/Blunder.ScriptsCatalog/` | Scan game DLL → `.blunder/behaviour_catalog.json` |
| `engine/src/runtime/function/script/scripts_builder.cpp` | After ok build, run catalog tool |
| `engine/src/runtime/function/script/behaviour_type_catalog.h/.cpp` | Load/parse catalog JSON |
| `engine/src/runtime/function/editor/editor_commands.h/.cpp` | Add/Remove/Reorder/SetProperty Commands |
| `engine/src/runtime/function/slint/inspector_panel.slint` | Behaviour section UI |
| `engine/src/runtime/function/slint/slint_system.*` | Sync/apply Behaviour Inspector |
| `engine/src/tests/object_behaviour_bag_test.cpp` | Bag + reorder |
| `engine/src/tests/behaviour_type_catalog_test.cpp` | Catalog parse fixture |
| `engine/src/tests/inspector_behaviour_commands_test.cpp` | Command undo/redo |

```
Scripts build ──► ScriptsCatalog ──► behaviour_catalog.json
                                            │
Inspector Add/Form ◄── BehaviourTypeCatalog ┘
        │
   Editor Commands ──► Object slots (type/id/bag/order)
        │
   exportToScene / save ──► .scene.asset behaviours[]
        │
   Player mountSceneBehaviours ──► AttachBehaviour + Apply bag
```

---

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
  - `bool Object::moveBehaviour(size_t from_index, size_t to_index)` — moves item; `to_index` is insertion index before move adjust (document in code: clamp; no-op if same)
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

- [ ] **Step 2: Run test — expect fail**

```powershell
cmake --build build/vs2026-debug --config Debug --target object_behaviour_bag_test
```

Expected: compile error — missing `setBehaviourProperties` / `moveBehaviour`.

- [ ] **Step 3: Minimal implementation**

In `object.h` `BehaviourSlot`:

```cpp
    eastl::vector<SceneBehaviourProperty> properties;
```

Include `runtime/function/scene/scene.h` (or extract `SceneBehaviourProperty` to `behaviour_property_bag.h` if include cycle — prefer extract if needed).

Implement get/set/move; in `scene_instance.cpp` after successful `restoreBehaviour`, assign `decl.properties` onto the slot via set API; in `exportToScene` copy `*getBehaviourProperties` into `decl.properties`.

- [ ] **Step 4: Run test — expect pass**

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\object_behaviour_bag_test.exe
```

Expected: `object_behaviour_bag_test: OK`

Also extend or run existing `scene_serializer_test` / mount test if bag export covered — optional follow-up in same task: tiny instantiate→export assert in this test via SceneInstance if lightweight.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/object/object.h engine/src/runtime/core/object/object.cpp engine/src/runtime/function/scene/scene_instance.cpp engine/src/tests/object_behaviour_bag_test.cpp engine/src/tests/CMakeLists.txt
git commit -m "feat: store Behaviour property bags on Object slots and support reorder"
```

---

### Task 2: Behaviour type catalog tool + native reader

**Files:**
- Create: `engine/managed/Blunder.ScriptsCatalog/Blunder.ScriptsCatalog.csproj`
- Create: `engine/managed/Blunder.ScriptsCatalog/Program.cs`
- Modify: `engine/src/runtime/function/script/scripts_builder.cpp` (invoke after ok)
- Create: `engine/src/runtime/function/script/behaviour_type_catalog.h`
- Create: `engine/src/runtime/function/script/behaviour_type_catalog.cpp`
- Create: `engine/src/tests/fixtures/behaviour_catalog_sample.json`
- Create: `engine/src/tests/behaviour_type_catalog_test.cpp`
- Modify: `engine/src/runtime/CMakeLists.txt`, `engine/src/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: game DLL path from `ScriptsBuildResult::output_dll`
- Produces:
  - JSON shape:
```json
{
  "types": [
    {
      "clr_name": "Game.Motor",
      "members": [
        { "name": "Speed", "kind": "number" },
        { "name": "Label", "kind": "string" },
        { "name": "EnabledFlag", "kind": "bool" }
      ]
    }
  ]
}
```
  - `struct BehaviourCatalogMember { eastl::string name; enum class Kind { Bool, Number, String } kind; };`
  - `struct BehaviourCatalogType { eastl::string clr_name; eastl::vector<BehaviourCatalogMember> members; };`
  - `bool loadBehaviourTypeCatalog(const std::filesystem::path& json_path, eastl::vector<BehaviourCatalogType>& out, eastl::string& error);`
  - Output path: `<project>/.blunder/behaviour_catalog.json`

- [ ] **Step 1: Failing native catalog parse test**

```cpp
// behaviour_type_catalog_test.cpp — load fixture, expect Motor + Speed number
expect_true("load", loadBehaviourTypeCatalog(fixture, types, err));
expect_true("has motor", /* find clr_name Game.Motor */);
```

- [ ] **Step 2: Run — fail missing API**

- [ ] **Step 3: Implement reader + catalog tool**

`Program.cs` sketch:

```csharp
// Args: <game.dll> <out.json>
// Load assembly path with MetadataLoadContext or Assembly.LoadFrom in collectible ALC
// Find non-abstract types assignable to Blunder.Behaviour (resolve Blunder.Api from same dir)
// Public instance fields/properties: bool, float/double/int/long, string
// Write JSON
```

Stage tool beside `bin/Debug` like other managed projects (mirror `blunder_api` CMake custom command pattern).

`scripts_builder.cpp` after `ok`:

```cpp
  // Run: dotnet <Blunder.ScriptsCatalog.dll> <output_dll> <project>/.blunder/behaviour_catalog.json
  // Non-fatal on catalog failure: log warning, leave prior catalog
```

- [ ] **Step 4: Tests pass**

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_type_catalog_test
.\build\vs2026-debug\engine\src\tests\Debug\behaviour_type_catalog_test.exe
```

- [ ] **Step 5: Commit**

```bash
git commit -m "feat: Behaviour type catalog scanner and native JSON loader"
```

---

### Task 3: ensureBoundObject + History Commands

**Files:**
- Modify: `engine/src/runtime/function/scene/scene_instance.h/.cpp`
- Modify: `engine/src/runtime/function/editor/editor_commands.h/.cpp`
- Create: `engine/src/tests/inspector_behaviour_commands_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `Object* SceneInstance::findBoundObject(EntityId) const;`
  - `Object* SceneInstance::ensureBoundObject(EntityId);` — create+bind+track in `m_bound_object_ids` if missing
  - `makeAddBehaviourCommand(scene, entity, clr_type, selection_before, selection_after)`
  - `makeRemoveBehaviourCommand(scene, entity, behaviour_id, ...)` — snapshot type+bag+index for undo
  - `makeReorderBehavioursCommand(scene, entity, from, to, ...)`
  - `makeSetBehaviourPropertyCommand(scene, entity, behaviour_id, key, before_variant, after_variant, ...)`

Command pattern (Add):

```cpp
class AddBehaviourCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{};
  eastl::string clr_type;
  BehaviourId created_id{k_invalid_behaviour_id};

  void redo() override {
    Object* object = scene->ensureBoundObject(entity_id);
    if (object == nullptr) return;
    if (!isValid(created_id)) {
      created_id = object->addBehaviour(clr_type);
    } else {
      object->restoreBehaviour(created_id, clr_type);
    }
  }
  void undo() override {
    Object* object = scene->findBoundObject(entity_id);
    if (object) object->removeBehaviour(created_id);
  }
};
```

Apply edit **before** `history.push` (same as Transform: caller mutates then pushes already-applied command — match existing `makeSetEntityTransformCommand` usage). Document: factories only store snapshots; caller applies then push, **or** command redo applies on push — **match existing Transform pattern** (store before/after; undo/redo apply). For Add: capture `created_id` after caller adds, undo removes, redo restores with same id via `restoreBehaviour`.

- [ ] **Step 1: Failing command test** (SceneInstance with one entity, DocumentHistory push Add, undo, redo)

- [ ] **Step 2: Run — fail**

- [ ] **Step 3: Implement ensure + four factories + test**

- [ ] **Step 4: Pass**

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\inspector_behaviour_commands_test.exe
```

- [ ] **Step 5: Commit**

```bash
git commit -m "feat: Behaviour Add/Remove/reorder/property Document History commands"
```

---

### Task 4: Inspector Slint section + slint_system wiring

**Files:**
- Modify: `engine/src/runtime/function/slint/inspector_panel.slint`
- Modify: `engine/src/runtime/function/slint/slint_system.h/.cpp` (and `editor_window.slint` if panel hosted there)
- Optionally: small `inspector_behaviour_ops.h` pure helpers for mapping catalog→rows

**Interfaces:**
- Consumes: catalog loader, Commands, `ensureBoundObject`, selection EntityId
- Produces: UI callbacks `on-add-behaviour(type)`, `on-remove-behaviour(id)`, `on-reorder(from,to)`, `on-property-commit(id,key,value)`

- [ ] **Step 1: Extend Slint model**

Add to `InspectorPanel` (sketch):

```slint
    struct BehaviourPropRow {
        key: string,
        kind: string, // "bool" | "number" | "string"
        bool-value: bool,
        number-value: float,
        string-value: string,
        missing-type: bool,
    }
    struct BehaviourRow {
        behaviour-id: int,
        type-name: string,
        missing: bool,
        props: [BehaviourPropRow],
    }
    in property <[BehaviourRow]> behaviours: [];
    in property <[string]> behaviour-type-choices: [];
    callback add-behaviour(string);
    callback remove-behaviour(int);
    callback reorder-behaviour(int, int);
    callback commit-behaviour-prop(int, string, string, float, bool);
```

Render section below Transform: list rows, drag handle if Slint version supports (else temporary up/down calling same reorder callback — prefer drag per grill; if blocked document in report).

- [ ] **Step 2: Build editor — UI compiles** (may not fully wire)

- [ ] **Step 3: `syncInspectorBehavioursFromSelection` / apply handlers**

Load catalog from `project_root/.blunder/behaviour_catalog.json`. For each Object behaviour: if type not in catalog → `missing=true`. Property rows from catalog members; values from bag. On Add: `ensureBoundObject`, `addBehaviour`, push Add command. Mark document dirty via existing edit system path.

- [ ] **Step 4: Smoke**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
# Manual: open project → select entity → Add Behaviour → edit → Ctrl+Z → Save → Reload
```

- [ ] **Step 5: Commit**

```bash
git commit -m "feat: Inspector Behaviour section with catalog-driven forms"
```

---

### Task 5: Mount bag preference + docs verify

**Files:**
- Modify: `engine/src/runtime/function/script/scene_behaviour_mount.cpp` (apply from Object bag when present)
- Touch: `CONTEXT.md` / ADR 0016 only if names drifted

- [ ] **Step 1: Adjust mount to read Object-slot bag first** (so Edit Mode bags apply on Play without relying only on original Scene asset pointer)

- [ ] **Step 2: Run**

```powershell
cmake --build build/vs2026-debug --config Debug --target object_behaviour_bag_test behaviour_type_catalog_test inspector_behaviour_commands_test
.\build\vs2026-debug\engine\src\tests\Debug\object_behaviour_bag_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\behaviour_type_catalog_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\inspector_behaviour_commands_test.exe
```

Expected: all OK.

- [ ] **Step 3: Commit if mount/docs changed**

```bash
git commit -m "fix: apply Behaviour property bags from Object slots on mount"
```

---

## Self-review

| Spec / grill | Task |
|--------------|------|
| Declaration-authoritative Edit Mode | 3–4 |
| Catalog after build | 2 |
| bool/number/string + catalog forms | 2, 4 |
| History Add/Remove/reorder/property | 3–4 |
| Missing type keep | 4 |
| Bag on Object + export | 1 |
| First Add ensures Object | 3 |
| Mount still Player | unchanged path; Task 5 bag source |

Placeholder scan: no TBD steps. Types consistent: `BehaviourCatalogType`, Commands, `ensureBoundObject`.
