# Camera Preview (Viewport PiP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Edit Mode shows a Unity-style **Camera Preview** floating panel over the viewport — a live secondary render through a selected scene **Camera Component**, with Slint chrome (drag / resize / collapse / Collapse menu), without showing it in the Player.

**Architecture:** Resolve preview camera from selection → build view/proj (reuse `resolvePlayCamera` math with aspect from content box) → render scene into a dedicated offscreen RT via `ForwardRenderPath::renderFrameTo(..., draw_overlays=false)` with shadows off → CPU-present into Slint `camera-preview-image` → `CameraPreviewPanel` over the viewport tile. Panel rect blocks viewport pick/orbit. Layout memory is process-local only.

**Tech Stack:** C++20, Vulkan offscreen RT, `ForwardRenderPath`, Slint UI, `EditorSelectionSystem`, CTest.

## Global Constraints

- Glossary: `CONTEXT.md` — **Camera Preview**, **Camera Component**, **Editor Overlay**, **Editor Camera**, **Main Camera**.
- Visibility: selection includes any Camera; target = primary-if-Camera else first selected Camera.
- Live secondary render; no Editor Overlays in preview; aspect = content box; longest edge ≤ 480.
- Slint chrome + independent image (not baked into `viewport-image`).
- Drag, resize (min ~160×90), collapse; menu = Collapse/Expand only; in-process layout memory.
- Panel rect blocks pick/orbit; Player never shows Camera Preview.
- TDD for resolve / RT size / matrix helpers; frequent scoped commits.
- OpenSpec change: `camera-preview` (`openspec/changes/camera-preview/`).

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/camera_preview_resolve.h` | Pure: selection → preview EntityId |
| `engine/src/runtime/function/render/overlay/camera_preview_rt_size.h` | Pure: content size → RT size (≤480 long edge) |
| `engine/src/runtime/function/render/forward/forward_render_path.h/.cpp` | `renderFrameTo` + `draw_overlays` flag |
| `engine/src/runtime/function/render/render_system.h/.cpp` | Own preview RT; second pass; readback present |
| `engine/src/runtime/function/slint/camera_preview_panel.slint` | Floating chrome UI |
| `engine/src/runtime/function/slint/editor_window.slint` | Host panel in viewport tile |
| `engine/src/runtime/function/slint/slint_system.h/.cpp` | Image + layout property sync |
| `engine/src/runtime/function/render/overlay/navigate_gizmo_layout.h` (or new `camera_preview_layout.h`) | Hit-test panel rect |
| `engine/src/runtime/CMakeLists.txt` | Register new `.slint` |
| `engine/src/tests/camera_preview_resolve_test.cpp` | Resolve TDD |
| `engine/src/tests/camera_preview_rt_size_test.cpp` | RT size TDD |
| `CONTEXT.md` | Glossary term |
| `docs/adr/0018-camera-preview-secondary-offscreen.md` | ADR |
| `openspec/changes/camera-preview/` | Specs / tasks |

```
Editor viewport tile
┌─────────────────────────────────────────────┐
│ [TransformToolbar]              [Nav gizmo] │
│                                             │
│              Editor Camera view             │
│                                             │
│                    ┌──────────────────────┐ │
│                    │ Title  [≡ Collapse]  │ │
│                    │ ┌──────────────────┐ │ │
│                    │ │ Camera Preview   │ │ │
│                    │ │ (2nd offscreen)  │ │ │
│                    │ └──────────────────┘ │ │
│                    └──────────────────────┘ │
└─────────────────────────────────────────────┘
         ▲ Slint chrome     ▲ not in viewport-image pixels
```

---

### Task 0: Branch + domain docs

**Files:**
- Modify: `CONTEXT.md` (near **Camera Gizmo** / **Editor Overlay**)
- Create: `docs/adr/0018-camera-preview-secondary-offscreen.md`

- [ ] **Step 1: Branch**

```bash
git checkout -b feat/camera-preview
```

- [ ] **Step 2: Glossary** — insert after **Camera Gizmo** block:

```md
**Camera Preview**:
An authorship-only floating panel over the editor viewport that shows a live view through a selected scene **Camera Component** (pose + FOV + near/far). It is Slint chrome plus a dedicated preview image, not an OverlaySystem draw into the main viewport offscreen, and never appears in the Player.
_Avoid_: Game View dock; Play window; Editor Camera widget; baking the PiP into `viewport-image`
```

- [ ] **Step 3: ADR** — record: dedicated secondary offscreen + second Slint image; rejected blit-into-main and OverlaySystem-only chrome.

- [ ] **Step 4: Commit**

```bash
git add CONTEXT.md docs/adr/0018-camera-preview-secondary-offscreen.md openspec/changes/camera-preview
git commit -m "$(cat <<'EOF'
docs: Camera Preview glossary, ADR, OpenSpec scaffold

EOF
)"
```

---

### Task 1: Resolve preview camera (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/overlay/camera_preview_resolve.h`
- Create: `engine/src/tests/camera_preview_resolve_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt` (mirror `align_camera_target_test` block)

**Interfaces:**

```cpp
#pragma once

#include "EASTL/span.h"

#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

struct CameraPreviewTargetResult {
  bool ok{false};
  EntityId entity_id{k_invalid_entity_id};
};

/// Primary if it has a Camera; else first selected entity that has a Camera.
/// Empty selection or no Camera in selection → ok=false.
inline CameraPreviewTargetResult resolveCameraPreviewTarget(
    const SceneInstance& scene, EntityId primary_id,
    eastl::span<const EntityId> selected_ids) {
  CameraPreviewTargetResult result{};
  if (selected_ids.empty()) {
    return result;
  }
  auto has_camera = [&](EntityId id) {
    return id != k_invalid_entity_id && !scene.isTombstoned(id) &&
           scene.getCamera(id) != nullptr;
  };
  if (has_camera(primary_id)) {
    result.ok = true;
    result.entity_id = primary_id;
    return result;
  }
  for (EntityId id : selected_ids) {
    if (has_camera(id)) {
      result.ok = true;
      result.entity_id = id;
      return result;
    }
  }
  return result;
}

}  // namespace Blunder
```

- [ ] **Step 1: Write failing tests** in `camera_preview_resolve_test.cpp` (same expect helpers style as `play_camera_resolve_test.cpp`):

```cpp
// empty selection -> !ok
// primary has camera -> that id
// primary mesh, second selected camera -> second id
// no cameras in selection -> !ok
// tombstoned camera skipped
```

- [ ] **Step 2: Run test — expect FAIL** (header missing)

```powershell
cmake --build build/vs2026-debug --config Debug --target camera_preview_resolve_test
# link/compile fails until header exists
```

- [ ] **Step 3: Add header + CMake `add_executable` / `add_test`**

- [ ] **Step 4: Run — expect PASS**

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\camera_preview_resolve_test.exe
```

- [ ] **Step 5: Commit** `feat(preview): resolve Camera Preview target from selection`

---

### Task 2: Preview RT size clamp (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/overlay/camera_preview_rt_size.h`
- Create: `engine/src/tests/camera_preview_rt_size_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

**Interfaces:**

```cpp
#pragma once

#include <algorithm>
#include <cstdint>

namespace Blunder {

inline constexpr uint32_t kCameraPreviewMaxLongEdgePx = 480;

struct CameraPreviewRtSize {
  uint32_t width{0};
  uint32_t height{0};
  bool ok{false};
};

/// Map logical content size to GPU RT size; preserve aspect; clamp longest edge.
inline CameraPreviewRtSize computeCameraPreviewRtSize(float content_w,
                                                      float content_h) {
  CameraPreviewRtSize out{};
  if (!(content_w > 0.5f) || !(content_h > 0.5f)) {
    return out;
  }
  float w = content_w;
  float h = content_h;
  const float long_edge = std::max(w, h);
  if (long_edge > static_cast<float>(kCameraPreviewMaxLongEdgePx)) {
    const float s = static_cast<float>(kCameraPreviewMaxLongEdgePx) / long_edge;
    w *= s;
    h *= s;
  }
  out.width = std::max(1u, static_cast<uint32_t>(w + 0.5f));
  out.height = std::max(1u, static_cast<uint32_t>(h + 0.5f));
  out.ok = true;
  return out;
}

}  // namespace Blunder
```

- [ ] **Step 1: Tests** — 320×180 stays; 1920×1080 → long edge 480; 0×0 → !ok
- [ ] **Step 2: Implement + CMake + PASS**
- [ ] **Step 3: Commit** `feat(preview): clamp Camera Preview RT longest edge`

---

### Task 3: Matrices via existing Play resolve

**Files:**
- No new matrix file required if call site builds one `PlayCameraResolveInput` and calls `resolvePlayCamera(&input, 1, aspect)`.
- Optional thin wrapper test in `camera_preview_resolve_test.cpp` or extend `play_camera_resolve_test.cpp` — prefer a one-liner helper:

```cpp
// camera_preview_resolve.h (same file or sibling)
inline ResolvedPlayCamera buildCameraPreviewMatrices(const SceneInstance& scene,
                                                     EntityId entity_id,
                                                     float aspect) {
  ResolvedPlayCamera empty{};
  const CameraComponent* cam = scene.getCamera(entity_id);
  if (cam == nullptr || scene.isTombstoned(entity_id)) {
    return empty;
  }
  PlayCameraResolveInput input{};
  input.entity_id = entity_id;
  input.world = scene.getWorldMatrix(entity_id);
  input.camera = *cam;
  return resolvePlayCamera(&input, 1, aspect);
}
```

- [ ] **Step 1: Unit test** — known FOV/aspect → `projection[1][1]` sign flip present; entity position feeds view
- [ ] **Step 2: Commit** `feat(preview): build Camera Preview view/proj from CameraComponent`

---

### Task 4: `ForwardRenderPath::renderFrameTo`

**Files:**
- Modify: `engine/src/runtime/function/render/forward/forward_render_path.h`
- Modify: `engine/src/runtime/function/render/forward/forward_render_path.cpp`

**Interfaces:**

```cpp
/// Renders opaque+transparent into `target`. When draw_overlays is false, skips
/// OverlaySystem (required for Camera Preview). Uses `target` extent for viewport.
void renderFrameTo(rhi::IOffscreenRenderTarget* target,
                   const ForwardFrameState& frame_state,
                   const ForwardOpaqueDraw* opaque_draws,
                   uint32_t opaque_draw_count,
                   const ForwardOpaqueDraw* transparent_draws,
                   uint32_t transparent_draw_count,
                   uint32_t frame_index,
                   bool draw_overlays);

/// Existing renderFrame becomes:
///   renderFrameTo(m_offscreen, ..., /*draw_overlays=*/true);
```

Implementation notes:
- Copy body of current `renderFrame`; replace `m_offscreen` with `target`.
- Gate `m_overlay_system->draw_scene_overlays` on `draw_overlays && m_overlay_system`.
- Shadow pass: still gated by `frame_state.shadows_enabled` — preview call site sets `false` so shared shadow map is not rewritten mid-frame.

- [ ] **Step 1: Refactor** `renderFrame` → call `renderFrameTo(m_offscreen, …, true)`
- [ ] **Step 2: Build** `engine_runtime` — expect success; main viewport still draws overlays
- [ ] **Step 3: Commit** `refactor(render): ForwardRenderPath::renderFrameTo for secondary targets`

---

### Task 5: RenderSystem owns preview RT + second pass

**Files:**
- Modify: `engine/src/runtime/function/render/render_system.h/.cpp`
- Modify: present path near `UIViewportBridge` / `SlintSystem` (add `setCameraPreviewImage`)

**Behavior each `tickVulkan` after main present path is scheduled:**

1. Read selection from `EditorSelectionSystem` + active `SceneInstance`.
2. `resolveCameraPreviewTarget` → if !ok or Slint reports collapsed → skip (optionally clear image once).
3. Content size from Slint-synced logical content w/h → `computeCameraPreviewRtSize`.
4. Resize `m_camera_preview_offscreen` if needed (`createOffscreenTarget` same as main).
5. `aspect = rt_w / rt_h`; `buildCameraPreviewMatrices`; fill `ForwardFrameState` from resolved (copy shading from main frame; `shadows_enabled=false`; set viewport_width/height to RT).
6. Same command buffer or a follow-up: `m_forward_path->renderFrameTo(preview_rt, preview_state, opaque, …, transparent, …, frame_index, false)`.
7. GPU→CPU readback of preview color (can start with a simplified single-slot staging copy mirroring `UIViewportBridge` enqueue/map) → `SlintSystem::setCameraPreviewImage(pixels, w, h)`.
8. Force main viewport redraw when preview is active so scene edits refresh both (`m_force_viewport_render` or generation bump).

Member fields (sketch):

```cpp
eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_camera_preview_offscreen;
bool m_camera_preview_collapsed{false};
float m_camera_preview_content_w{320.f};
float m_camera_preview_content_h{180.f};
```

- [ ] **Step 1: Allocate / shutdown preview RT with RenderSystem**
- [ ] **Step 2: Wire second pass + readback + Slint setter stub**
- [ ] **Step 3: Build `engine_editor`**
- [ ] **Step 4: Commit** `feat(render): secondary offscreen pass for Camera Preview`

---

### Task 6: Slint `CameraPreviewPanel`

**Files:**
- Create: `engine/src/runtime/function/slint/camera_preview_panel.slint`
- Modify: `engine/src/runtime/function/slint/editor_window.slint` (inside `tile.active-panel-kind == 1` after `TransformToolbar`)
- Modify: `engine/src/runtime/CMakeLists.txt` — add `"function/slint/camera_preview_panel.slint"` to `slint_target_sources`

**Panel sketch:**

```slint
export component CameraPreviewPanel inherits Rectangle {
    in-out property <bool> preview-visible: false;
    in-out property <bool> collapsed: false;
    in property <string> title: "Camera";
    in property <image> preview-image;
    in-out property <length> panel-x;
    in-out property <length> panel-y;
    in-out property <length> content-w: 320px;
    in-out property <length> content-h: 180px;
    callback collapse-toggled;
    // TouchArea title drag; edge/corner resize; PopupMenu with Collapse
}
```

Wire on `editor_window`:
- `in property <bool> camera-preview-visible`
- `in property <image> camera-preview-image`
- `in-out` layout props synced from C++
- Default position: `panel-x = parent.width - content-w - 10px`, `panel-y = parent.height - content-h - title_h - 10px` on first show

- [ ] **Step 1: Component + import + host in viewport tile**
- [ ] **Step 2: CMake + rebuild Slint/`engine_editor`**
- [ ] **Step 3: Commit** `feat(ui): Camera Preview Slint floating panel chrome`

---

### Task 7: SlintSystem sync + present API

**Files:**
- Modify: `engine/src/runtime/function/slint/slint_system.h/.cpp`

```cpp
void setCameraPreviewImage(const uint8_t* pixels_rgba, uint32_t width,
                           uint32_t height);
void syncCameraPreviewFromEngine(/* visible, title, collapsed, rect */);
// Pull collapsed + content size + panel rect from Slint each frame for render/hit
```

- [ ] **Step 1: Property setters for image / visibility / title**
- [ ] **Step 2: Read back collapsed + content size + panel x/y/w/h for C++**
- [ ] **Step 3: Commit** `feat(ui): sync Camera Preview image and layout with Slint`

---

### Task 8: Block viewport pick / orbit over panel

**Files:**
- Create or extend: `engine/src/runtime/function/render/overlay/camera_preview_layout.h`

```cpp
inline bool hitCameraPreviewPanelLocal(float local_x, float local_y,
                                       float panel_x, float panel_y,
                                       float panel_w, float panel_h) {
  return local_x >= panel_x && local_x < panel_x + panel_w &&
         local_y >= panel_y && local_y < panel_y + panel_h;
}
```

- Modify: `slint_system.cpp` / `render_system.cpp` / viewport input path where `hitViewportTopRightChromeLocal` is consulted — also return early for Camera Preview rect (including collapsed title-bar height).

- [ ] **Step 1: Helper + call sites for pick and camera drag**
- [ ] **Step 2: Manual check — click on panel does not select mesh**
- [ ] **Step 3: Commit** `fix(input): Camera Preview panel blocks viewport pick`

---

### Task 9: Validate + docs touch-up

- [ ] **Step 1: Run unit tests**

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\camera_preview_resolve_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\camera_preview_rt_size_test.exe
```

- [ ] **Step 2: Build editor + player**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
cmake --build build/vs2026-debug --config Debug --target engine_player
```

- [ ] **Step 3: USER-VERIFY checklist**

1. Select Camera → bottom-right panel appears with entity name; live scene view.
2. Move / rotate Camera or drag FOV on gizmo → preview updates.
3. Multi-select: primary Camera wins; primary mesh + selected Camera → first Camera in selection.
4. Drag / resize / collapse; menu Collapse works; collapsed stops updating image.
5. Click on panel → no mesh pick; orbit does not start on panel.
6. Restart editor → panel back to default bottom-right.
7. Player: no Camera Preview chrome.

- [ ] **Step 4: Mark OpenSpec tasks done; commit** `test(preview): Camera Preview USER-VERIFY notes`

---

## Self-review (writing-plans)

| Spec requirement | Task |
|------------------|------|
| Visibility / primary-else-first | Task 1 |
| Live overlay-free render + aspect + ≤480 + collapse stop | Tasks 2–5 |
| Slint chrome drag/resize/collapse/menu | Tasks 6–7 |
| Block pick | Task 8 |
| Player never | Task 5/6 gated editor-only; Task 9 verify |
| Glossary + ADR | Task 0 |
| Not baked into viewport-image | Tasks 5–6 (second image) |

No TBD placeholders; interfaces named consistently (`resolveCameraPreviewTarget`, `computeCameraPreviewRtSize`, `renderFrameTo`, `setCameraPreviewImage`).
