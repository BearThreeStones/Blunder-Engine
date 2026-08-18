## 1. Pure helpers + TDD

- [x] 1.1 Add `ground_placement.{h,cpp}` (`groundPlacementFromRay`) and `content_browser_drop.{h,cpp}` (classify + cursor resolve)
- [x] 1.2 Add `PlacementPreviewController` visibility/pose/clear API (no Entity)
- [x] 1.3 Add `engine/src/tests/placement_preview_test.cpp` and register it in `engine/src/tests/CMakeLists.txt`
- [x] 1.4 Wire new `.cpp` files into `engine/src/runtime/CMakeLists.txt`
- [x] 1.5 RED then GREEN: build + run `placement_preview_test`

## 2. Render submit + spawn share Ground placement

- [x] 2.1 Extract `submitStandaloneMeshToRender` from scene render bridge
- [x] 2.2 `PlacementPreviewController` loads `MeshAsset` when visible and submits after `syncSceneToRender`
- [x] 2.3 `EditorSceneEditSystem::spawnMeshAsset` uses `groundPlacementFromRay` / shared window helper
- [x] 2.4 Engine tick appends preview draws; `requestViewportRedraw` on pose/visibility change

## 3. Cursor, Escape, drag wiring

- [x] 3.1 Apply three-state system cursor while Content Browser drag is active; clear on end
- [x] 3.2 Update preview pose from pointer motion (`isPointerOverViewport` + Ground placement)
- [x] 3.3 Escape cancels Content Browser drag (no spawn / open / reparent) and clears preview + cursor
- [x] 3.4 Drop / drag-end still spawn or reparent as today and clear preview + cursor

## 4. Validation

- [x] 4.1 Build `placement_preview_test` and `engine_editor`
- [x] 4.2 Manual QA:
  - [x] Mesh Asset drag over viewport: follow-mesh + pointer cursor
  - [x] Leave viewport: preview hides; folder hover: move cursor
  - [x] Scene Asset over viewport: pointer, no preview; drop opens
  - [x] Inspector/chrome: not-allowed
  - [x] Escape cancels; drop still spawns and is undoable
