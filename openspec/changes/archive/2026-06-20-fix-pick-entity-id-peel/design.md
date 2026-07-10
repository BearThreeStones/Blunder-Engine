## Context

- §1–9 **implemented**; QA: 10.2 ✅, 10.3 ✅, 10.5 ✅, **10.4 ❌**.
- Poll-before-render + `markViewportDirtyRegion` fixed menu flash but **not** first async left-click outline.
- Cycle and menu: `applySelection` **before** `rendererTick` → outline/gizmo OK.
- First left-click: selection only when async `multi_peel` finishes inside `rendererTick` → hierarchy updates, viewport image lags.

## Goals / Non-Goals

**Goals:**

- **10.4:** First left-click shows outline + gizmo without camera move (static camera, wait ≥200ms).
- Keep 10.3 cycle, 10.5 menu, entity-ID peel, async peel for full hit list.
- Minimal behavior change: front-most selection same as today; cycle list still from async peel.

**Non-Goals:**

- Promote to `Box*`; P2 broad-phase; sync full multi-peel on every click (blocks GPU).

## Decisions

### 1–10. Prior decisions

Entity-ID peel, poll-before-render, menu isolation, selection dirty — **implemented**. Still required for peel in flight and menu.

### 11. Sync-then-async left-click (Solution A)

**Decision:** On `onViewportLeftReleased` when issuing a new `multi_peel` (not local cycle branch):

1. **Sync front-most pick** — `RenderSystem::pickEntityAtWindowPosition` or `pickAllEntitiesAtWindowPosition` first hit, then `promotePickEntity`.
2. If valid hit → **`applySelection` immediately** (input phase, before next `rendererTick`) — same timing as cycle/menu.
3. **`requestPick(multi_peel)`** — async entity-ID peel runs as today (1 pass/frame).
4. **On async delivery** — `deliverPickResult`:
   - Build `dedupePromotedPickHits` → store in `m_last_peel_hits`, update `m_last_click_pixel`.
   - **Do not** `applySelection` when promoted front equals current `getPrimarySelection()` (already synced).
   - If async peel list differs only in depth (same promoted front), no selection churn.
   - If sync missed but async hits (edge), apply front from async as fallback.

**Rationale:** Matches proven sync paths (cycle/menu). Async peel cost unchanged; only selection timing moves.

**Alternatives:**

- Force Slint composite on async delivery only — rejected (10.4 failed after poll-before-render).
- Poll in `engine.cpp` before `rendererTick` — insufficient alone per QA.

### 12. PickRequestKind (optional)

**Decision:** Prefer **no new enum** if `deliverPickResult` can branch on `repeat_click` / sync-already-applied flag (`m_sync_pick_pending` or compare selection to peel front). Optional `PickRequestKind::peel_list_only` if branching stays messy.

### 13. Cycle path unchanged

Local same-pixel cycle (lines 175–190) still calls `applySelection` synchronously; no sync pick on that branch.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Double GPU work (sync readback + async peel) | Sync is one 1×1 readback; acceptable for editor click |
| Sync/async front mismatch (stale camera 1 frame) | Rare; async delivery applies if different |
| `setSelection` early return if same entity | Sync + async same front → no redundant redraw |

## Migration Plan

1. Implement sync pick + immediate `applySelection` in `onViewportLeftReleased`.
2. Adjust `deliverPickResult` / `pickWithCyclingFromResult` for peel-list-only async completion.
3. Update `render-pipeline.md`.
4. Manual QA 10.4; confirm 10.3/10.5 regressions.

## Open Questions

- Sync API: `pickEntityAtWindowPosition` vs first hit of `pickAllAtWindowPosition`? **Default: `pickAll` front + promote** (consistent with peel list).
