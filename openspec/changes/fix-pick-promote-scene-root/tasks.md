## 1. Promotion walk-up

- [x] 1.1 Replace `promotePickEntity` in `pick_entity_promotion.h` with scene-root-child walk-up (stop when parent has invalid grandparent)
- [x] 1.2 Add unit tests: `node_prim0 → BoxFront`, root mesh unchanged, two-level parent unchanged, invalid leaf handling
- [x] 1.3 Verify `dedupePromotedPickHits` and `promotePickEntityList` inherit new semantics without API changes

## 2. Viewport pick sync peel list

- [x] 2.1 In `onViewportLeftReleased`, after sync pick + `applySelection`, call `pickAllEntitiesAtWindowPosition` and assign `dedupePromotedPickHits` result to `m_last_peel_hits`
- [x] 2.2 Confirm async `multi_peel` completion still refreshes `m_last_peel_hits` without double-changing primary selection when front hit matches
- [x] 2.3 Confirm local same-pixel cycle branch uses promoted scene-root-child ids from cached list

## 3. Piercing menu

- [x] 3.1 Audit piercing menu gather path applies `promotePickEntity` / `dedupePromotedPickHits` before row labels and ids
- [ ] 3.2 Manual: Ctrl+right-click overlap pixel in pick_test shows `BoxFront`, `BoxMid`, `BoxBack`

## 4. pick_test QA and docs

- [x] 4.1 Verify pick_test scene loads with world bounds and camera frames AABB (±Y overlap visible)
- [ ] 4.2 Manual QA: viewport click `BoxFront` mesh → Hierarchy highlights `BoxFront`, gizmo + outline visible
- [ ] 4.3 Manual QA: second click same pixel cycles to `BoxMid` then `BoxBack` (±Y view)
- [x] 4.4 Update `docs/agents/render-pipeline.md` promotion rule (scene-root-child, supersede immediate-parent-only)

## 5. Validation

- [x] 5.1 Build `engine_editor` Debug
- [x] 5.2 Run promotion unit tests
- [x] 5.3 `openspec validate fix-pick-promote-scene-root`
