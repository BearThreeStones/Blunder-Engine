# Player Gameplay Camera + Authorship Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Player accepts only Gameplay Input (no Editor Camera / authorship tools) and renders exclusively through a scene **Camera Component**, with Play preflight failing when the entry scene has no valid Camera.

**Architecture:** Extend host-mode policy (same idea as `editor_overlay_policy.h`) to disable all Player authorship input including `EditorCamera::onEvent` and viewport pick. Add `CameraComponent` on entities (MeshRenderer-style), persist it on `SceneEntityDefinition`, resolve Main/first Camera into view/projection for Player ticks only. Edit Mode keeps `EditorCamera`. Wire Play camera check into the existing Play start gate beside Scripts dirty.

**Tech Stack:** C++20, `SceneInstance` / `SceneSerializer`, `RenderSystem`, `PlaySessionController` / `play_preflight`, Slint Inspector (thin), CTest.

## Global Constraints

- Glossary: `CONTEXT.md` — **Camera Component**, **Main Camera**, **Editor Camera**, **Play camera preflight**, **Player** authorship input, **Play Pause** (no orbit).
- Player input: Gameplay Input + system window chrome only.
- Player view: scene Camera only — never Editor Camera interaction (including Pause).
- Edit Mode viewport: still Editor Camera.
- Multi-camera: Main Camera flag wins; else first valid Camera; none → Play preflight fails (do not spawn Player).
- Authoring MVP: serialize + Inspector FOV/near/far/Main + one Add path; no camera tool gizmo suite.
- DogWalk: still no camera-follow Behaviour; static Main Camera entity is OK.
- TDD for policy + resolve + serializer + preflight helpers; frequent scoped commits.
- Prefer OpenSpec change `player-gameplay-camera` (single change covering isolation + Camera).

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/editor_overlay_policy.h` | Keep overlays gate; add or include authorship-input helper |
| `engine/src/runtime/function/render/player_authorship_input.h` | `playerAuthorshipInputEnabled(EngineHostMode)` |
| `engine/src/runtime/function/scene/camera_component.h` | `CameraComponent` POD |
| `engine/src/runtime/function/scene/play_camera_resolve.h` | Pure resolve Main/first → matrices |
| `engine/src/runtime/function/scene/scene.h` | Optional camera fields on `SceneEntityDefinition` |
| `engine/src/runtime/function/scene/scene_instance.h/.cpp` | Store/get cameras |
| `engine/src/runtime/function/scene/scene_serializer.cpp` | JSON round-trip |
| `engine/src/runtime/function/scene/scene_system.cpp` | Apply camera on load |
| `engine/src/runtime/function/render/render_system.cpp` | Player: authorship off; view from resolve |
| `engine/src/runtime/project/play_preflight.h/.cpp` | `sceneHasPlayCamera` / gate helper |
| `engine/src/runtime/project/play_session_controller.cpp` / `ui_host.cpp` | Call camera preflight before spawn |
| Inspector Slint + `slint_system.cpp` | Thin Camera section + Add Camera |
| Tests under `engine/src/tests/` | Policy, resolve, serializer, preflight |

```
Edit Mode                         Play Mode (Player)
─────────────────────             ─────────────────────────
EditorCamera ON                   EditorCamera interaction OFF
Editor Overlays ON                Overlays OFF (already)
Authorship pick/gizmo ON          Authorship input OFF
view = EditorCamera               view = resolvePlayCamera(scene)
                                  no Camera → Play blocked
```

---

### Task 0 (optional): OpenSpec scaffold

**Files:**
- Create: `openspec/changes/player-gameplay-camera/{proposal,design,tasks}.md`
- Create: `openspec/changes/player-gameplay-camera/specs/{play-player,editor-overlays,play-mode}/spec.md` deltas as needed

- [ ] **Step 1:** `openspec new change "player-gameplay-camera"`
- [ ] **Step 2:** Capture grilled decisions from this plan + `CONTEXT.md`
- [ ] **Step 3:** Commit OpenSpec-only if using a feature branch

---

### Task 1: Player authorship input policy (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/player_authorship_input.h`
- Create: `engine/src/tests/player_authorship_input_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/render_system.cpp` (`onEvent`)

**Interfaces:**
- Consumes: `EngineHostMode`
- Produces:
  ```cpp
  namespace Blunder {
  /// Editor Camera, pick, gizmos, and other authorship input in the Player.
  inline bool playerAuthorshipInputEnabled(EngineHostMode host_mode) {
    return host_mode != EngineHostMode::Player;
  }
  }
  ```

- [ ] **Step 1: Write failing test**

```cpp
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/render/player_authorship_input.h"
#include <cstdio>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); ++g_failures; }
}
}

int main() {
  using namespace Blunder;
  expect_true("editor allows authorship input",
              playerAuthorshipInputEnabled(EngineHostMode::Editor));
  expect_true("player blocks authorship input",
              !playerAuthorshipInputEnabled(EngineHostMode::Player));
  if (g_failures) return 1;
  std::printf("player_authorship_input_test: all passed\n");
  return 0;
}
```

- [ ] **Step 2: Register CMake target** (mirror `editor_overlay_policy_test` — header-only include path `engine/src`, no `engine_runtime` required)

- [ ] **Step 3: Build — expect fail missing header**

```powershell
cmake --build build/vs2026-debug --config Debug --target player_authorship_input_test
```

Expected: cannot open `player_authorship_input.h`

- [ ] **Step 4: Add header** (exact API above)

- [ ] **Step 5: Build + run — expect PASS**

- [ ] **Step 6: Gate `RenderSystem::onEvent`**

Include `player_authorship_input.h`. After existing overlay gate, wrap **all remaining authorship paths** that still run in Player:

1. Viewport pick start / pointer move / left-click pick / piercing (any `m_viewport_pick` handling)
2. `m_editor_camera->onEvent(event)` — **must not run in Player**
3. Any EditorCamera key shortcuts still reached via `onEvent`

Keep: RenderDoc F11 if present; do not gate Gameplay Input sampling in `engine.cpp` (already `player_host && focused && !paused`).

Pattern:

```cpp
const bool authorship =
    playerAuthorshipInputEnabled(g_runtime_global_context.hostMode());

if (authorship && m_editor_camera) {
  m_editor_camera->onEvent(event);
  // ...
}

if (authorship && !event.handled && m_editor_camera && /* pick blocks */) {
  // existing pick code
}
```

Also call `m_editor_camera->setInteractionLocked(true)` once when `hostMode()==Player` during `RenderSystem::initialize` (defense in depth).

- [ ] **Step 7: Build `engine_player`**

- [ ] **Step 8: Commit**

```bash
git add engine/src/runtime/function/render/player_authorship_input.h \
        engine/src/runtime/function/render/render_system.cpp \
        engine/src/tests/player_authorship_input_test.cpp \
        engine/src/tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
fix(player): disable authorship input and Editor Camera in Player

EOF
)"
```

---

### Task 2: CameraComponent + resolvePlayCamera (TDD)

**Files:**
- Create: `engine/src/runtime/function/scene/camera_component.h`
- Create: `engine/src/runtime/function/scene/play_camera_resolve.h`
- Create: `engine/src/tests/play_camera_resolve_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace Blunder {
  struct CameraComponent final {
    float vertical_fov_degrees{45.0f};
    float near_clip{0.1f};
    float far_clip{1000.0f};
    bool is_main{false};
  };

  struct PlayCameraResolveInput {
    // One candidate per entity that has a CameraComponent.
    EntityId entity_id{k_invalid_entity_id};
    Mat4 world{1.0f};
    CameraComponent camera{};
  };

  struct ResolvedPlayCamera {
    bool ok{false};
    EntityId entity_id{k_invalid_entity_id};
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Vec3 position{0.0f};
    Vec3 forward{0.0f, 1.0f, 0.0f};
    float near_clip{0.1f};
    float far_clip{1000.0f};
    float vertical_fov_radians{glm::radians(45.0f)};
  };

  /// Prefer is_main; else first entry in array order. Empty → ok=false.
  ResolvedPlayCamera resolvePlayCamera(
      const PlayCameraResolveInput* cameras, size_t count, float aspect);
  }
  ```

View convention (Z-up, match forward render):

```cpp
// world columns: X=right, Y=forward-ish basis, Z=up-ish depending on author rotation.
// Use glTF-style local look along -Z of the entity basis:
const Vec3 position = Vec3(world[3]);
const Vec3 forward = glm::normalize(Vec3(-world[2]));
const Vec3 up = kWorldUp; // from math_types / coordinate helpers
Mat4 view = glm::lookAt(position, position + forward, up);
Mat4 projection = glm::perspective(vertical_fov_radians, aspect, near, far);
projection[1][1] *= -1.0f; // Vulkan Y flip — same as RenderSystem fallback path
```

- [ ] **Step 1: Failing tests** in `play_camera_resolve_test.cpp`:

  1. empty → `!ok`
  2. one camera → `ok`, entity id matches
  3. two cameras, second `is_main` → picks second
  4. two cameras, none main → picks first
  5. FOV/near/far copied into result

- [ ] **Step 2: Build fail / implement headers / PASS**

- [ ] **Step 3: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(scene): add CameraComponent and resolvePlayCamera

EOF
)"
```

---

### Task 3: Scene definition + serializer round-trip

**Files:**
- Modify: `engine/src/runtime/function/scene/scene.h` — add optional camera on `SceneEntityDefinition`
- Modify: `engine/src/runtime/function/scene/scene_serializer.cpp` (+ `.h` if needed)
- Modify: `engine/src/tests/scene_serializer_test.cpp` (or new focused cases)

**Interfaces:**
- Produces on `SceneEntityDefinition`:
  ```cpp
  bool has_camera{false};
  CameraComponent camera{};
  ```
- JSON shape (per entity object):
  ```json
  "camera": {
    "verticalFovDegrees": 45.0,
    "nearClip": 0.1,
    "farClip": 1000.0,
    "isMain": true
  }
  ```
  Absent key → `has_camera == false`.

- [ ] **Step 1: Extend serializer test** — serialize entity with camera, deserialize, assert fields; entity without camera stays `!has_camera`

- [ ] **Step 2: Implement parse/write** next to mesh/behaviours parsing in `scene_serializer.cpp`

- [ ] **Step 3: Run**

```powershell
cmake --build build/vs2026-debug --config Debug --target scene_serializer_test
# run scene_serializer_test.exe
```

Expected: PASS

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(scene): serialize CameraComponent on scene entities

EOF
)"
```

---

### Task 4: SceneInstance storage + load attach

**Files:**
- Modify: `engine/src/runtime/function/scene/scene_instance.h/.cpp`
- Modify: `engine/src/runtime/function/scene/scene_system.cpp` (where mesh attach runs)

**Interfaces:**
- Produces:
  ```cpp
  void SceneInstance::setCamera(EntityId id, CameraComponent camera);
  const CameraComponent* SceneInstance::getCamera(EntityId id) const;
  // iterate: forEachCamera(Fn) mirroring forEachMeshRenderer
  ```

- [ ] **Step 1: When creating entities from `SceneEntityDefinition`, if `has_camera` call `setCamera`**

- [ ] **Step 2: Helper used by render/preflight:**

```cpp
// play_camera_resolve.h or scene_instance helper
inline ResolvedPlayCamera resolvePlayCameraFromScene(
    const SceneInstance& scene, float aspect) {
  eastl::vector<PlayCameraResolveInput> cams;
  scene.forEachCamera([&](EntityId id, const CameraComponent& cam) {
    PlayCameraResolveInput in;
    in.entity_id = id;
    in.world = scene.getWorldMatrix(id);
    in.camera = cam;
    cams.push_back(in);
  });
  return resolvePlayCamera(cams.data(), cams.size(), aspect);
}
```

- [ ] **Step 3: Build `engine_runtime` / smoke load scene with camera JSON**

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(scene): attach CameraComponent on scene load

EOF
)"
```

---

### Task 5: RenderSystem Player uses resolved Camera

**Files:**
- Modify: `engine/src/runtime/function/render/render_system.cpp` (`tickVulkan` camera block ~916+)

**Interfaces:**
- Consumes: `resolvePlayCameraFromScene`, `g_runtime_global_context.hostMode()`, active `SceneInstance`

- [ ] **Step 1: Branch view source**

```cpp
const bool player =
    g_runtime_global_context.hostMode() == EngineHostMode::Player;
if (player) {
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;
  const float aspect =
      static_cast<float>(offscreen_extent.width) /
      static_cast<float>(eastl::max(1u, offscreen_extent.height));
  ResolvedPlayCamera cam =
      scene ? resolvePlayCameraFromScene(*scene, aspect) : ResolvedPlayCamera{};
  if (!cam.ok) {
    // Should be unreachable if preflight works; skip draw / clear only.
    pollViewportPresent();
    return;
  }
  view = cam.view;
  projection = cam.projection;
  camera_position = cam.position;
  camera_forward = cam.forward;
  near_clip = cam.near_clip;
  far_clip = cam.far_clip;
  vertical_fov = cam.vertical_fov_radians;
  // Do NOT call m_editor_camera->onUpdate / snapFocus in Player.
} else if (m_editor_camera) {
  // existing EditorCamera path unchanged
}
```

- [ ] **Step 2: Build `engine_player` + `engine_editor`**

- [ ] **Step 3: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(player): render Play view from scene CameraComponent

EOF
)"
```

---

### Task 6: Play camera preflight

**Files:**
- Modify: `engine/src/runtime/project/play_preflight.h/.cpp`
- Modify: `engine/src/tests/play_preflight_test.cpp` (or new `play_camera_preflight_test.cpp`)
- Modify: `engine/src/runtime/function/ui/ui_host.cpp` `startPlaySession` **and/or** `PlaySessionController::play` hooks

**Interfaces:**
- Produces:
  ```cpp
  /// True if scene JSON/text (or loaded Scene) contains ≥1 camera.
  bool sceneAssetHasPlayCamera(const Scene& scene);

  struct PlayCameraGateResult {
    bool ok{false};
    std::string error;
  };
  PlayCameraGateResult runPlayCameraGate(const Scene& scene);
  ```
  `runPlayCameraGate`: `ok=false`, `error="play entry scene has no Camera"` when none.

- [ ] **Step 1: Unit test** — Scene with/without `has_camera`

- [ ] **Step 2: Implement gate**

- [ ] **Step 3: Call before spawn** in `startPlaySession` after scene path known:

```cpp
// Load scene asset via AssetManager (same path Play will use) OR parse from
// FileSystem read of active scene. Prefer AssetManager->loadScene(path)->getScene().
const auto scene_asset = /* load */;
if (!scene_asset) { /* existing error path */ }
const PlayCameraGateResult cam = runPlayCameraGate(scene_asset->getScene());
if (!cam.ok) {
  LOG_ERROR("[Play] aborted: {}", cam.error);
  return false;
}
```

Do **not** spawn Player when gate fails.

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(play): preflight require CameraComponent before Player spawn

EOF
)"
```

---

### Task 7: Inspector thin Camera authoring + Add

**Files:**
- Modify: Inspector Slint (`inspector` section in editor window / dedicated fragment)
- Modify: `engine/src/runtime/function/slint/slint_system.cpp` (+ `.h` as needed)
- Modify: `EditorSceneEditSystem` or selection helpers to `setCamera` / clear

**Interfaces:**
- When single selection has camera: show FOV, near, far, Main checkbox; commits write `CameraComponent` + `markDirty` + history if transform-history pattern is easy — **YAGNI:** markDirty + write-through OK if History Command for camera is heavy; prefer Document History only if an existing component-edit command pattern is one file away.
- **Add Camera:** button/menu on selection → `setCamera(id, CameraComponent{})` with `is_main=true` if scene has no main yet.
- Persist via existing scene save (`has_camera` on definition when saving active scene — ensure save path copies instance cameras back into `Scene` / asset).

- [ ] **Step 1: Trace save path** — `EditorSceneEditSystem::saveActiveScene` must emit `"camera"` for entities that have `getCamera`. Extend export accordingly.

- [ ] **Step 2: Minimal Slint fields + callbacks**

- [ ] **Step 3: Manual** — Add Camera on empty/mesh entity, save, reload, fields stick

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(editor): Inspector CameraComponent fields and Add Camera

EOF
)"
```

---

### Task 8: Content seed + dual-window verification

**Files:**
- Modify: project Test entry scene used for Play (e.g. `E:/Blunder Projects/Test/Assets/Scenes/pick_test.scene.asset` or DogWalk scene) — add entity with Main Camera looking at content
- Verify: `CONTEXT.md` already updated in grilling — no regress

- [ ] **Step 1: Add Main Camera entity** to the Play entry scene JSON (position above/back from origin, `isMain: true`)

- [ ] **Step 2: Manual checklist**

1. Edit Mode: Editor Camera orbit still works; overlays visible  
2. Play without Camera (temp remove) → Play aborted with error  
3. Play with Main Camera → Player shows scene from Camera; **no** RMB/MMB orbit; WASD moves character only when Player focused (Gameplay Input)  
4. Pause → still no orbit; Tick frozen  
5. Editor while Playing → edits do not live-sync; editor orbit still works in editor  

- [ ] **Step 3: Commit content + any doc nits**

```bash
git commit -m "$(cat <<'EOF'
content: seed Main Camera for Play entry scene

EOF
)"
```

---

## Self-review

**Spec coverage**

| Requirement | Task |
|-------------|------|
| Player authorship input off | Task 1 |
| Editor Camera interaction off in Player | Task 1 |
| Camera Component + Main/first resolve | Task 2–4 |
| Player renders from Camera | Task 5 |
| Preflight no Camera | Task 6 |
| Inspector + Add | Task 7 |
| Pause no orbit | Tasks 1 + 5 (no EditorCamera update) |
| Edit Mode keeps EditorCamera | Task 5 else-branch |
| Content seed | Task 8 |

**Placeholder scan:** None intentional.

**Type consistency:** `CameraComponent`, `resolvePlayCamera`, `resolvePlayCameraFromScene`, `runPlayCameraGate`, `playerAuthorshipInputEnabled` named consistently across tasks.

**Out of scope:** Camera follow Behaviour, camera gizmo toolbox, forcing editor viewport to Main Camera, live sync, env override for authorship input.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-27-player-gameplay-camera.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — fresh subagent per task, review between tasks  
2. **Inline Execution** — this session with executing-plans checkpoints  

**Which approach?**
