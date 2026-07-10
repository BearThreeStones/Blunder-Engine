# Hybrid GPU Picking P1+P2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-click full-scene CPU mesh scans and depth-peel multi-hit with a GPU instance buffer (P1) and compute broad-phase + candidate narrow pick (P2), fixing `pick_test` left-click selection, outline/gizmo visibility, and same-pixel cycling.

**Architecture:** P1 caches pickable `PickDraw` rows and `PickInstanceGpu` SSBO data when the scene is dirty (same cadence as `syncSceneToRender`), so `HybridGpuPickSystem::requestPick` never calls `forEachMeshRenderer`. P2 adds `pick_broad_phase.slang` to produce a sorted broad hit list (≤1024), runs narrow raster only on candidates, uses the broad list for piercing menu and click cycling, and applies selection via `deliverPickResult` with sync peel removed from the release path.

**Tech Stack:** C++20, Vulkan (SSBO, compute dispatch, fence async), Slang shaders, EASTL, glm, existing `PickOverlay` / `HybridGpuPickSystem`, OpenSpec change `hybrid-gpu-picking`.

**OpenSpec:** `openspec/changes/hybrid-gpu-picking/` (P0 done; start at tasks §2).

**Blocked:** Do not continue `fix-viewport-left-click-async-pick` band-aids.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/pick/pick_instance.h` | `PickInstanceGpu`, flags, draw-index constants |
| `engine/src/runtime/function/render/pick/pick_instance_buffer.h` | `PickInstanceBuffer` public API |
| `engine/src/runtime/function/render/pick/pick_instance_buffer.cpp` | CPU rebuild from scene, world AABB, cached `PickDraw[]` |
| `engine/src/runtime/function/render/pick/pick_broad_hits.h` | CPU broad-hit record, sort/dedupe helpers |
| `engine/src/runtime/function/render/pick/pick_broad_phase.h/.cpp` | Vulkan compute pipeline + SSBO readback (P2) |
| `engine/shaders/pick_broad_phase.slang` | Ray vs AABB compute (P2) |
| `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.{h,cpp}` | Orchestrate instance buffer, broad, narrow, peel removal |
| `engine/src/runtime/function/render/overlay/pick_overlay.{h,cpp}` | Accept external draw list; deprecate click-time `collectPickableDraws` |
| `engine/src/runtime/function/render/overlay/overlay_system.{h,cpp}` | Own `PickInstanceBuffer`, rebuild hook |
| `engine/src/runtime/function/editor/viewport_pick_system.cpp` | Remove sync pick on release; consume broad list |
| `engine/src/tests/pick_instance_buffer_test.cpp` | CPU rebuild tests |
| `engine/src/tests/pick_broad_hits_test.cpp` | Sort/dedupe tests |
| `docs/agents/render-pipeline.md` | Hybrid architecture docs (P2) |

---

## Part A — P1: GPU pick instance buffer

### Task 1: `PickInstanceGpu` layout

**Files:**
- Create: `engine/src/runtime/function/render/pick/pick_instance.h`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`
- Test: `engine/src/tests/pick_instance_buffer_test.cpp` (stub)

- [ ] **Step 1: Add header**

```cpp
#pragma once

#include <cstdint>

namespace Blunder {

enum PickInstanceFlags : uint32_t {
  k_pick_instance_pickable = 1u << 0,
};

/// std430-friendly row for GPU broad phase (P2) and metadata (P1).
struct PickInstanceGpu {
  uint32_t entity_id{0};
  uint32_t parent_id{0};
  uint32_t flags{0};
  uint32_t draw_index{0};
  float aabb_min[3]{0.0f, 0.0f, 0.0f};
  float aabb_pad0{0.0f};
  float aabb_max[3]{0.0f, 0.0f, 0.0f};
  float aabb_pad1{0.0f};
};

static_assert(sizeof(PickInstanceGpu) == 48, "PickInstanceGpu size must match shader");

}  // namespace Blunder
```

- [ ] **Step 2: Register in CMake**

Add to `engine/src/runtime/function/render/CMakeLists.txt` in the `engine_runtime` source list:

```cmake
    "function/render/pick/pick_instance.h"
```

- [ ] **Step 3: Commit**

```bash
git add engine/src/runtime/function/render/pick/pick_instance.h engine/src/runtime/function/render/CMakeLists.txt
git commit -m "feat(pick): add PickInstanceGpu layout for hybrid picking P1"
```

---

### Task 2: CPU `PickInstanceBuffer` rebuild (failing test first)

**Files:**
- Create: `engine/src/runtime/function/render/pick/pick_instance_buffer.h`
- Create: `engine/src/runtime/function/render/pick/pick_instance_buffer.cpp`
- Create: `engine/src/tests/pick_instance_buffer_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

```cpp
// engine/src/tests/pick_instance_buffer_test.cpp
#include <cstdio>

#include "runtime/function/render/pick/pick_instance_buffer.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"

int main() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId root = scene.createEntity("Box", Vec3{}, Quat{}, Vec3{});
  MeshRendererComponent renderer{};
  renderer.mesh = eastl::make_shared<MeshAsset>(
      Asset::Meta{}, eastl::vector<MeshVertex>{}, eastl::vector<uint32_t>{});
  scene.addComponent(root, eastl::move(renderer));

  PickInstanceBuffer buffer;
  buffer.rebuild(scene, /*render_system=*/nullptr);

  if (buffer.pickDraws().size() != 1u) {
    std::fprintf(stderr, "FAIL: expected 1 pick draw, got %zu\n",
                 buffer.pickDraws().size());
    return 1;
  }
  if (buffer.gpuInstances().size() != 1u) {
    std::fprintf(stderr, "FAIL: expected 1 gpu instance\n");
    return 1;
  }
  if (buffer.gpuInstances()[0].entity_id != static_cast<uint32_t>(root)) {
    std::fprintf(stderr, "FAIL: entity_id mismatch\n");
    return 1;
  }

  std::printf("pick_instance_buffer_test: all passed\n");
  return 0;
}
```

Add to `engine/src/tests/CMakeLists.txt`:

```cmake
add_executable(pick_instance_buffer_test pick_instance_buffer_test.cpp)
target_link_libraries(pick_instance_buffer_test PRIVATE engine_runtime cgltf)
if(MSVC)
  target_compile_options(pick_instance_buffer_test PRIVATE /Zc:preprocessor)
endif()
add_test(NAME pick_instance_buffer_test COMMAND pick_instance_buffer_test)
set_tests_properties(pick_instance_buffer_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug")
```

- [ ] **Step 2: Run test — expect FAIL (link error)**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target pick_instance_buffer_test --config Debug
e:\Dev\Blunder-Engine\build\vs2026-debug\engine\src\tests\Debug\pick_instance_buffer_test.exe
```

Expected: build fails — `pick_instance_buffer.h` not found.

- [ ] **Step 3: Implement minimal buffer**

`pick_instance_buffer.h`:

```cpp
#pragma once

#include "EASTL/vector.h"
#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/pick/pick_instance.h"

namespace Blunder {

class RenderSystem;
class SceneInstance;

class PickInstanceBuffer final {
 public:
  void rebuild(SceneInstance& scene, RenderSystem* render_system);
  const eastl::vector<PickOverlay::PickDraw>& pickDraws() const { return m_draws; }
  const eastl::vector<PickInstanceGpu>& gpuInstances() const { return m_instances; }
  uint32_t generation() const { return m_generation; }

 private:
  eastl::vector<PickOverlay::PickDraw> m_draws;
  eastl::vector<PickInstanceGpu> m_instances;
  uint32_t m_generation{0};
};

}  // namespace Blunder
```

`pick_instance_buffer.cpp` — copy filter logic from `PickOverlay::collectPickableDraws` (opaque / mask, skip blend, require gpu mesh). For world AABB when `render_system` is null in test, use unit AABB at origin; when non-null, transform `mesh->getLocalBounds()` by `scene.getWorldMatrix(entity_id)` via 8-corner transform.

- [ ] **Step 4: Add sources to CMake**

```cmake
    "function/render/pick/pick_instance_buffer.cpp"
    "function/render/pick/pick_instance_buffer.h"
```

- [ ] **Step 5: Run test — expect PASS**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target pick_instance_buffer_test --config Debug
e:\Dev\Blunder-Engine\build\vs2026-debug\engine\src\tests\Debug\pick_instance_buffer_test.exe
```

Expected: `pick_instance_buffer_test: all passed`

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/function/render/pick/pick_instance_buffer.* engine/src/tests/pick_instance_buffer_test.cpp engine/src/tests/CMakeLists.txt engine/src/runtime/function/render/CMakeLists.txt
git commit -m "feat(pick): add PickInstanceBuffer CPU rebuild with unit test"
```

---

### Task 3: Wire rebuild on scene dirty (not on click)

**Files:**
- Modify: `engine/src/runtime/function/render/overlay/overlay_system.h`
- Modify: `engine/src/runtime/function/render/overlay/overlay_system.cpp`
- Modify: `engine/src/runtime/function/scene/scene_instance.h` (optional `markPickInstancesDirty()`)

- [ ] **Step 1: Add member to OverlaySystem**

```cpp
#include "runtime/function/render/pick/pick_instance_buffer.h"
// ...
  void rebuildPickInstancesIfNeeded(SceneInstance& scene, RenderSystem& render_system);
  const PickInstanceBuffer& pickInstances() const { return m_pick_instances; }
// private:
  PickInstanceBuffer m_pick_instances;
  bool m_pick_instances_dirty{true};
```

- [ ] **Step 2: Rebuild implementation**

```cpp
void OverlaySystem::rebuildPickInstancesIfNeeded(SceneInstance& scene,
                                                 RenderSystem& render_system) {
  if (!m_pick_instances_dirty && !scene.isWorldMatricesDirty()) {
    return;
  }
  m_pick_instances.rebuild(scene, &render_system);
  m_pick_instances_dirty = false;
}
```

Call from `RenderSystem::tickVulkan` **before** `pollViewportPickIfActive()`:

```cpp
if (m_overlay_system && g_runtime_global_context.m_scene_system) {
  SceneInstance* scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene) {
    m_overlay_system->rebuildPickInstancesIfNeeded(*scene, *this);
  }
}
```

Mark dirty when scene edits occur: call `m_overlay_system->markPickInstancesDirty()` from `syncSceneToRender` end or when `SceneInstance::markTransformsDirty()` path runs (simplest: set `m_pick_instances_dirty = true` at start of `syncSceneToRender` via new `OverlaySystem::markPickInstancesDirty()` invoked from `scene_render_bridge.cpp`).

- [ ] **Step 3: Build**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target engine_editor --config Debug
```

Expected: success.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(pick): rebuild PickInstanceBuffer on scene dirty, not per click"
```

---

### Task 4: HybridGpuPickSystem uses cached draws

**Files:**
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp`
- Modify: `engine/src/runtime/function/render/overlay/overlay_system.cpp`

- [ ] **Step 1: Change `beginGpuPass`**

Replace:

```cpp
m_draws = m_pick_overlay->collectPickableDraws(scene, render_system);
```

With:

```cpp
m_draws = render_system.getOverlaySystem()->pickInstances().pickDraws();
```

Add null/empty guard; return false if `m_draws.empty()` for miss.

- [ ] **Step 2: Verify with debug**

Set `BLUNDER_PICK_DEBUG=1`, click viewport, confirm log shows `requestPick` without hitting `collectPickableDraws` (temporary log in `collectPickableDraws` if needed, remove before commit).

- [ ] **Step 3: Build + manual smoke**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target engine_editor --config Debug
```

Launch editor, Ctrl+right on `pick_test` — piercing menu should still work.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(pick): HybridGpuPickSystem reads cached PickDraw list from P1 buffer"
```

---

### Task 5: Upload GPU SSBO (P1 foundation for P2)

**Files:**
- Modify: `engine/src/runtime/function/render/pick/pick_instance_buffer.h`
- Modify: `engine/src/runtime/function/render/pick/pick_instance_buffer.cpp`
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp`

- [ ] **Step 1: Add Vulkan SSBO to `PickInstanceBuffer`**

```cpp
void uploadToGpu(VulkanContext* ctx, VulkanAllocator* alloc);
VkBuffer gpuBuffer() const;
uint32_t gpuInstanceCount() const;
```

Upload `m_instances` via staging buffer when `rebuild` changes data. Store `eastl::unique_ptr<VulkanBuffer> m_gpu_buffer`.

- [ ] **Step 2: Call `uploadToGpu` after rebuild in `rebuildPickInstancesIfNeeded`**

- [ ] **Step 3: Build — no behavioral change yet**

Broad phase not wired; SSBO exists for P2.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(pick): upload PickInstanceGpu SSBO on rebuild (P1)"
```

---

### Task 6: Remove sync pick from left release (async-only until P2)

**Files:**
- Modify: `engine/src/runtime/function/editor/viewport_pick_system.cpp`
- Modify: `engine/src/runtime/function/editor/viewport_pick_system.h`

- [ ] **Step 1: Delete sync block in `onViewportLeftReleased`**

Remove lines that call `pickAllEntitiesAtWindowPosition`, `pickEntityAtWindowPosition`, and `m_sync_pick_applied`. Non-cycle branch becomes:

```cpp
m_last_click_pixel = click_pixel;
m_cycle_index = 0;
requestPick(window_x, window_y, modifier_flags, PickRequestKind::multi_peel);
```

Remove `m_sync_pick_applied` member and related `deliverPickResult` guards.

- [ ] **Step 2: Build**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target engine_editor --config Debug
```

- [ ] **Step 3: Commit**

```bash
git commit -m "refactor(pick): remove sync peel from left release (P1 async-only)"
```

**P1 manual QA (task 2.6):** Profiler or breakpoint — `PickOverlay::collectPickableDraws` must not run during `requestPick`; only during `PickInstanceBuffer::rebuild`.

---

## Part B — P2: Compute broad phase + candidate narrow

### Task 7: Broad hit CPU helpers (test first)

**Files:**
- Create: `engine/src/runtime/function/render/pick/pick_broad_hits.h`
- Create: `engine/src/tests/pick_broad_hits_test.cpp`

- [ ] **Step 1: Failing test for sort + promoted dedupe**

```cpp
struct BroadHit { EntityId entity_id; float t; };

eastl::vector<BroadHit> sortBroadHitsByDistance(eastl::vector<BroadHit> hits);
eastl::vector<EntityId> promoteAndDedupeBroadHits(SceneInstance& scene,
    const eastl::vector<BroadHit>& hits);
```

Test: three hits with t=3,1,2 → sorted 1,2,3; two leaves promoting to same parent → one entry.

- [ ] **Step 2: Implement helpers using `promotePickEntity`**

- [ ] **Step 3: Run test PASS, commit**

```bash
git commit -m "feat(pick): add broad hit sort and promoted dedupe helpers"
```

---

### Task 8: `pick_broad_phase.slang` compute shader

**Files:**
- Create: `engine/shaders/pick_broad_phase.slang`
- Modify: `engine/shaders/CMakeLists.txt` (add to `SHADER_CANDIDATES` if pick_prepass not listed, add `pick_broad_phase.slang`)

- [ ] **Step 1: Shader skeleton**

```slang
struct PickInstanceGpu { uint entityId; uint parentId; uint flags; uint drawIndex;
    float3 aabbMin; float aabbPad0; float3 aabbMax; float aabbPad1; };

struct PickBroadUniforms { float3 rayOrigin; float rayPad0; float3 rayDir; float rayPad1;
    uint instanceCount; uint maxHits; uint hitCountOut; uint pad0; };

struct BroadHitRecord { uint entityId; float t; };

[[vk::binding(0, 0)]] ConstantBuffer<PickBroadUniforms> ubo;
[[vk::binding(1, 0)]] StructuredBuffer<PickInstanceGpu> instances;
[[vk::binding(2, 0)]] RWStructuredBuffer<BroadHitRecord> hitRecords;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> hitCount;

// [numthreads(64,1,1)]
// void main(uint3 id : SV_DispatchThreadID) { ray-AABB slab test; atomicAdd hitCount; }
```

Mirror `geometry.h` `intersect(Ray, AABB)` slab logic. Cap at `min(atomicAdd, maxHits)`.

- [ ] **Step 2: Copy shader to build output** (CMake deploy_shaders target)

- [ ] **Step 3: Commit**

```bash
git commit -m "feat(pick): add pick_broad_phase.slang compute shader"
```

---

### Task 9: `PickBroadPhase` Vulkan wrapper

**Files:**
- Create: `engine/src/runtime/function/render/pick/pick_broad_phase.h`
- Create: `engine/src/runtime/function/render/pick/pick_broad_phase.cpp`

- [ ] **Step 1: Follow `PickOverlay::createPipeline` pattern**

Use `SlangCompiler` to compile `engine/shaders/pick_broad_phase.slang` with `VK_SHADER_STAGE_COMPUTE_BIT`. Create descriptor set: instance SSBO, hit records SSBO, hit count, uniforms.

- [ ] **Step 2: API**

```cpp
void dispatch(VkCommandBuffer cmd, const Ray& ray, VkBuffer instance_buffer,
              uint32_t instance_count, VkBuffer hit_buffer, uint32_t max_hits);
PickFetchStatus readbackHits(eastl::vector<BroadHit>& out); // after fence
```

- [ ] **Step 3: Build, commit**

```bash
git commit -m "feat(pick): add PickBroadPhase Vulkan compute wrapper"
```

---

### Task 10: Integrate broad phase into `HybridGpuPickSystem`

**Files:**
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.h`
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp`

- [ ] **Step 1: Extend pick request state**

```cpp
eastl::vector<BroadHit> m_broad_hits;
eastl::vector<EntityId> m_broad_promoted_hits;
bool m_broad_ready{false};
```

- [ ] **Step 2: `requestPick` flow**

1. Build world ray from `EditorCamera` + window pixel (`resolvePickPixel`).
2. Submit compute broad phase + fence.
3. On fence complete in `poll`: readback hits → `sortBroadHitsByDistance` → `promoteAndDedupeBroadHits` → store in `m_broad_promoted_hits`.
4. Set `PickResult.peel_hits` from **leaf** ids (for promotion) or store promoted list separately.

- [ ] **Step 3: Narrow phase uses filtered draws**

```cpp
m_draws = filterPickDraws(cached_draws, broad_leaf_ids);
```

Single 1×1 narrow readback (no peel loop).

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(pick): integrate broad phase into HybridGpuPickSystem"
```

---

### Task 11: Viewport delivery — broad list + selection timing

**Files:**
- Modify: `engine/src/runtime/function/editor/viewport_pick_system.cpp`
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp`

- [ ] **Step 1: `deliverPickResult` for `multi_peel` / new `broad_multi` kind**

Option A (minimal enum churn): reuse `multi_peel` but `peel_hits` carries **promoted** broad list.

```cpp
if (result.kind == PickRequestKind::multi_peel) {
  m_last_peel_hits = result.peel_hits; // already promoted
  m_last_click_pixel = glm::vec2(result.window_x, result.window_y);
  const EntityId front = result.peel_hits.empty() ? k_invalid_entity_id : result.peel_hits.front();
  applySelection(front, result.modifier_flags);
}
```

- [ ] **Step 2: Move pick poll before render skip contributes to stale composite**

Ensure `pollViewportPickIfActive` runs at start of `tickVulkan` (already does). On `applySelection`, `EditorSelectionSystem::onSelectionChanged` already calls `markViewportDirtyRegion()` + `requestViewportRedraw()`.

Add after `applySelection` in `deliverPickResult`:

```cpp
if (g_runtime_global_context.m_render_system) {
  g_runtime_global_context.m_render_system->requestViewportRedraw();
}
```

- [ ] **Step 3: Same-pixel cycle uses `m_last_peel_hits` filled from broad list on first delivery**

Second click hits sync cycle branch (`repeat_click && m_last_peel_hits.size() > 1`) — no peel wait.

- [ ] **Step 4: Commit**

```bash
git commit -m "fix(pick): deliver broad hit list and apply selection from async delivery"
```

---

### Task 12: Remove depth-peel hot path

**Files:**
- Modify: `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp`
- Modify: `engine/src/runtime/function/render/overlay/pick_overlay.cpp` (keep sync API for dev, not called from release)

- [ ] **Step 1: Delete multi-pass peel loop in `poll`**

Remove `m_excluded_entities` peel chain; one broad + one narrow per request.

- [ ] **Step 2: Piercing menu uses same broad list**

`PickRequestKind::piercing_menu` → broad only (no narrow required for menu rows) OR narrow for front + broad for full list.

- [ ] **Step 3: Commit**

```bash
git commit -m "refactor(pick): remove iterative depth peel from hot path (P2)"
```

---

### Task 13: Documentation + OpenSpec tasks

**Files:**
- Modify: `docs/agents/render-pipeline.md`
- Modify: `openspec/changes/hybrid-gpu-picking/tasks.md`

- [ ] **Step 1: Document hybrid flow**

```markdown
| Operation | Implementation |
|-----------|----------------|
| Left-click | async broad + candidate narrow; broad list drives cycle |
| Multi-hit | GPU broad phase hit list (≤1024), not depth peel |
| Instance data | GPU SSBO rebuilt on scene dirty |
```

- [ ] **Step 2: Check off tasks 2.1–2.6, 3.1–3.7, 4.2–4.5 in OpenSpec**

- [ ] **Step 3: Commit**

```bash
git commit -m "docs(pick): hybrid GPU picking P1+P2 architecture"
```

---

### Task 14: Manual QA (`pick_test.scene.asset`)

- [ ] **Step 1: Build**

```powershell
cmake --build e:\Dev\Blunder-Engine\build\vs2026-debug --target engine_editor --config Debug
```

- [ ] **Step 2: Test checklist**

| # | Action | Expected |
|---|--------|----------|
| 4.2 | Left-click `BoxFront`, static camera | Hierarchy + outline + gizmo ≤2 frames |
| 4.3 | Same-pixel left-click ×2 | `BoxMid` → `BoxBack` in Hierarchy |
| 4.4 | Ctrl+right overlap pixel | ≥3 piercing rows |
| 4.5 | Orbit drag + release | No pick |
| 1.6 | Click glTF child leaf | Selects `BoxFront` / `BoxMid` / `BoxBack` (promoted) |

- [ ] **Step 3: Run unit tests**

```powershell
ctest --test-dir e:\Dev\Blunder-Engine\build\vs2026-debug -C Debug -R pick_
```

Expected: `pick_entity_promotion_test`, `pick_instance_buffer_test`, `pick_broad_hits_test` pass.

---

## Self-review (spec coverage)

| Spec requirement | Task |
|------------------|------|
| No per-click scene scan (P1) | Tasks 3–4 |
| GPU instance SSBO (P1) | Task 5 |
| Compute broad ray-AABB (P2) | Tasks 8–10 |
| Candidate-only narrow (P2) | Task 10 |
| Hit list cap 1024 (P2) | Task 8 |
| Parent promotion | Existing + Task 7 |
| Click vs drag guard | P0 done; verify Task 14 |
| Piercing menu broad list | Task 12 |
| Static-camera outline/gizmo | Task 11 + Task 14 |
| Async non-blocking release | Tasks 6, 10 |

**Placeholder scan:** None — all steps name concrete files and code.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-01-hybrid-gpu-picking-p1-p2.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration. REQUIRED SUB-SKILL: `superpowers:subagent-driven-development`.

2. **Inline Execution** — run tasks in one session via `superpowers:executing-plans`, batch with checkpoints.

**Which approach?**

To start implementation now, exit explore mode and run:

```
/opsx:apply hybrid-gpu-picking
```

Reference this plan file for step-by-step detail.
