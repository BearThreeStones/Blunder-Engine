## 0. Pivot (reject async-first)

- [x] 0.1 Record QA failure in design.md (done in artifact revision)
- [x] 0.2 Remove `onAsyncPickFirstPass` → `applySelection` from `hybrid_gpu_pick_system.cpp` and `viewport_pick_system.{h,cpp}`

## 1. Restore sync-then-async on left release

- [x] 1.1 On `onViewportLeftReleased` (non-cycle branch): sync front-most pick → `promotePickEntity` → `applySelection` (input phase)
- [x] 1.2 Sync-fill `m_last_peel_hits` via `pickAllEntitiesAtWindowPosition` (or equivalent) + `dedupePromotedPickHits` before `requestPick(multi_peel)`
- [x] 1.3 Then `requestPick(multi_peel)` as today

## 2. Async delivery (refine list only)

- [x] 2.1 `deliverPickResult(multi_peel)`: refresh `m_last_peel_hits` from full async peel
- [x] 2.2 Skip `applySelection` when promoted front equals primary (sync already applied)
- [x] 2.3 If sync missed and async front valid → `applySelection` fallback

## 3. Keep press/release relaxation

- [x] 3.1 Left-release pick without prior press (drag/orbit guards) — keep from async-first implementation
- [x] 3.2 Debug logs behind `BLUNDER_PICK_DEBUG=1` — keep

## 4. Docs

- [x] 4.1 Revert `docs/agents/render-pipeline.md` to sync-then-async wording (undo async-first edits)

## 5. Manual QA (`pick_test.scene.asset`)

- [ ] 5.1 First left-click `BoxFront`: Hierarchy + outline + gizmo ≤2 frames, **static camera**
- [ ] 5.2 Second same-pixel click: Hierarchy → `BoxMid` → `BoxBack` (even if viewport lags, Hierarchy must cycle)
- [ ] 5.3 Ctrl+right piercing menu: 3 rows; menu row select still works
- [ ] 5.4 Hierarchy direct click regression

## 6. Validation

- [x] 6.1 Build `engine_editor` Debug
- [x] 6.2 `openspec validate fix-viewport-left-click-async-pick`
