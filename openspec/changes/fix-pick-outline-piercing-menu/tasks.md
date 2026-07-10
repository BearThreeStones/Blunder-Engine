## 1. Sync-then-async left-click (viewport pick)

- [x] 1.1 On `onViewportLeftReleased` (non-cycle branch): sync `pickEntityAtWindowPosition` → `promotePickEntity` → `applySelection` in input phase
- [x] 1.2 Sync-fill `m_last_peel_hits` via `pickAllEntitiesAtWindowPosition` + `dedupePromotedPickHits` before `requestPick(multi_peel)`
- [x] 1.3 `deliverPickResult(multi_peel)`: refresh list from async; skip `applySelection` when promoted front equals primary; async fallback when sync missed
- [x] 1.4 Keep `requestViewportRedraw()` on async delivery when peel list updates
- [x] 1.5 Defer `requestPick(multi_peel)` to next `tickVulkan` after sync `applySelection` (avoid GPU/composite race on click frame)
- [x] 1.6 `markFullSkiaRefresh` bypasses `endFrame` idle composite pacing; viewport pick calls it from `notifyEditorUi`

## 2. Piercing menu Slint layout

- [x] 2.1 `piercing_menu.slint`: full-window transparent dismiss; `menu-panel` height from row layout (no `height: 100%` on panel)
- [x] 2.2 Verify `editor_window.slint` overlay z-order and `dialog-active` unchanged
- [ ] 2.3 Visual check: 3-row menu does not obscure Inspector/dock chrome

## 3. Docs

- [x] 3.1 Update `docs/agents/render-pipeline.md`: sync narrow on release + async broad list for peel/cycle

## 4. Manual QA (`pick_test.scene.asset`)

- [ ] 4.1 First left-click `BoxFront`: Hierarchy + outline + gizmo ≤2 frames, **static camera**
- [ ] 4.2 Second same-pixel click: cycle `BoxMid` → `BoxBack`
- [ ] 4.3 Ctrl+right piercing menu: ≥3 rows, **compact** panel at cursor
- [ ] 4.4 Orbit drag release: no mis-pick
- [ ] 4.5 Hierarchy direct-click regression

## 5. Validation

- [x] 5.1 Build `engine_editor` Debug
- [x] 5.2 `openspec validate fix-pick-outline-piercing-menu`
