## 1. Editor multi-selection

- [x] 1.1 Extend `EditorSelectionSystem` with `getSelectedIds()`, `addToSelection`, `removeFromSelection`, `toggleSelection`, `getPrimarySelection`, `isSelected`; keep `getSelection()` as primary alias
- [x] 1.2 Update `UiHost::dispatch` `selectEntity` to support modifier modes (replace / add / toggle) via new `UiEvent` fields or separate event kinds
- [x] 1.3 Plumb Shift/Ctrl from Hierarchy Slint callback (`on_hierarchy_entity_selected`) or pointer poll into selection dispatch
- [x] 1.4 Update `syncHierarchy` / `syncInspectorFromSelection` to highlight all selected rows; inspector continues to use primary selection only
- [x] 1.5 Audit call sites that assume single selection (`setSelection` only paths remain valid)

## 2. Packed object ID prepass

- [x] 2.1 Add `OutlineIdPack` helper (`color_id`, `ob_id` → `uint16_t`) matching design `(color_id << 14) | (ob_id & 0x3FFF)`
- [x] 2.2 Change `outline_prepass.slang` FS to write packed ID from uniform (per-draw `color_id` + `ob_id`) instead of constant `1`
- [x] 2.3 Extend `OutlineOverlay::CachedDraw` with `packed_id`; assign unique `ob_id` per selected root in `begin_sync` loop over `getSelectedIds()`
- [x] 2.4 Batch all selected roots' subtree meshes into one prepass clear/draw sequence
- [x] 2.5 Set `color_id = 0` while `TransformGizmoController::isDragging()`, else `color_id = 1`; expose dragging flag via `OverlayState` or direct controller read in `begin_sync`

## 3. Resolve shader upgrades

- [x] 3.1 Update `outline_resolve.slang` edge test: silhouette vs `0`, or `color_id` change; suppress edges when only `ob_id` differs at same `color_id`
- [x] 3.2 Add color lookup table in resolve for `color_id` 0 (transform orange) and 1 (object-select `#F57011`)
- [x] 3.3 Implement smooth edge coverage (cardinal ± optional diagonal samples + `smoothstep`) with depth occlusion unchanged
- [x] 3.4 Update `OutlineResolveUniformData` and descriptor uploads if new uniforms are required

## 4. Integration and docs

- [x] 4.1 Wire `OverlaySystem::begin_sync` / `OutlineOverlay` to multi-select selection source
- [x] 4.2 Build `engine_runtime` (Debug)
- [ ] 4.3 Manual: single select — smooth orange outline unchanged from v1 quality baseline
- [ ] 4.4 Manual: Shift-select two mesh entities — both outlined, no seam between co-selected touching hulls
- [ ] 4.5 Manual: drag gizmo — outline switches to transform orange, reverts on release
- [x] 4.6 Update `docs/agents/render-pipeline.md` with packed ID layout, multi-select, smooth resolve, drag color

## 5. Bug fixes (post-implementation)

- [x] 5.1 Fix resolve pixel lookup: use `SV_Position` instead of interpolated `uv * screenSize` so outline aligns with prepass ID buffer
- [x] 5.2 Unify resolve viewport / `screen_params` / render area extent with `OutlineTargets` (same as prepass)
