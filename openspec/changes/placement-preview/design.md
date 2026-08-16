## Context

Content Browser drag already exists (`ContentBrowserDragController`, viewport drop → `spawnAssetAtWindowPosition`, folder reparent). Spawn uses Ground placement (camera ray ∩ Z=0) and `loadMesh` + `MeshRendererComponent`. `syncSceneToRender` rebuilds opaque/transparent draws from scene MeshRenderers every engine tick. There is no transient draw, and the cursor stays default during Browser drag. See proposal.md for product rules.

Constraints: no scene Entity during drag; spawn Command only on drop; do not reuse Mesh Preview Render (studio lighting / offscreen); SDL3 has no copy cursor (pointer / move / not-allowed only).

## Goals / Non-Goals

**Goals:**

- Append a bind-pose MeshRenderer-equivalent draw after scene sync, posed at Ground placement.
- Pure, unit-tested drop classification, cursor resolve, Ground placement, and preview visibility.
- Drive cursor via existing `WindowSystem::setSystemCursor` / `clearSystemCursor`.
- Cancel on Escape next to the existing dock-drag Escape path.

**Non-Goals:**

- OS file drop onto the viewport.
- Surface snapping or view-plane billboards.
- Multi-primitive preview beyond what `loadMesh` / spawn already draws.
- Custom (non-system) cursors.
- Changing spawn/open/reparent drop semantics except sharing Ground placement.

## Decisions

### 1. Transient draw list, not a live Entity

**Decision:** `PlacementPreviewController` holds source path, loaded `MeshAsset`, visibility, and world position. After `syncSceneToRender`, `submitStandaloneMeshToRender` appends the same opaque/transparent path as scene meshes. Drop still calls `spawnMeshAsset`.

**Alternatives considered:**

- Spawn immediately and move the Entity until release — rejected; would dirty Outliner/history mid-drag.
- Mesh Preview Render into a 2D overlay — rejected; wrong lighting and not Ground placement.

### 2. Extract Ground placement and drop/cursor helpers

**Decision:** `groundPlacementFromRay` (Z=0, origin on miss) is shared by spawn and preview. `classifyContentBrowserDrop` + `resolveContentBrowserDragCursor` are pure functions. Visibility is `over_viewport && kind == mesh`.

**Alternatives considered:**

- Keep Ground placement as an anonymous helper in `editor_scene_edit_system.cpp` — rejected; preview and tests need the same rule.

### 3. Cursor owned by WindowSystem during Browser drag

**Decision:** While `ContentBrowserDragController::isDragging()`, apply the resolved SDL system cursor each motion (and on drag start/end). Clear on drop, Escape, or drag reset. Do not fight gizmo/dock cursors unless Browser drag is active (Browser drag wins).

**Alternatives considered:**

- Slint `mouse-cursor` on a full-window overlay — rejected; drag already leaves the Browser TouchArea.
- One MOVE cursor for the whole drag — rejected by product (three-state).

### 4. Load mesh on first viewport enter

**Decision:** Call `AssetManager::loadMesh` when Placement Preview becomes visible for a path; cache until source changes or drag ends. Load failure hides preview; drop still attempts spawn as today.

**Alternatives considered:**

- Load at drag threshold — extra hitch while still over the Browser.
- `collectMeshPreviewSubmeshes` — can enumerate more primitives than spawn's single `loadMesh`; keep spawn parity.

## Risks / Trade-offs

- **[Risk] Extra GPU upload on first viewport enter** → Mitigation: reuse `getOrUploadGpuMesh` cache; same mesh as drop will spawn.
- **[Risk] Tick clears draws then preview is forgotten** → Mitigation: append preview every tick after `syncSceneToRender` while visible.
- **[Risk] Cursor fights dock/gizmo** → Mitigation: apply Browser-drag cursor only while `isDragging()`; restore on end.
- **[Risk] Static-camera skip hides follow-mesh** → Mitigation: `requestViewportRedraw()` on pose/visibility change (editor-viewport-pacing delta).

## Migration Plan

Single change; no data migration. Rollback: revert. Validate with unit tests plus editor QA: Mesh follow-mesh, Scene pointer-without-preview, folder move cursor, Escape cancel, drop spawn + undo.

## Open Questions

- _(none)_
