## Why

`fix-viewport-left-click-async-pick` implemented **async-first** left-click (remove sync GPU on release; `onAsyncPickFirstPass` in `tickVulkan`). Manual QA on `pick_test.scene.asset` **failed**:

| Symptom | Result |
|---------|--------|
| First left-click → Hierarchy | ✅ Highlights (e.g. `BoxFront`) |
| First left-click → outline + gizmo (static camera) | ❌ Not visible until camera moves |
| Same-pixel second click → cycle | ❌ No Hierarchy change; no `BoxMid` / `BoxBack` |

This matches the **regression** documented in archived `2026-06-20-fix-pick-entity-id-peel` (10.4): selection inside `rendererTick` updates data but **Slint idle-tier composite** lags; sync-then-async was the proven fix. Removing sync also removed **immediate `m_last_peel_hits` fill**, so the cycle branch (`m_last_peel_hits.size() > 1`) never runs on the second click.

**Pivot:** Reject async-first. **Restore sync-then-async** on left release (input phase), keep async `multi_peel` for peel-list refinement only. Retain press/release relaxation from this change.

## What Changes

- **Restore sync-then-async on left release** — synchronous front-most pick + `promotePickEntity` + `applySelection` in the input phase; synchronous `pickAllEntitiesAtWindowPosition` (or equivalent) to **immediately** fill `m_last_peel_hits` for same-pixel cycling; then `requestPick(multi_peel)`.
- **Remove `onAsyncPickFirstPass` selection** — do not call `applySelection` from `HybridGpuPickSystem::poll` / inside `tickVulkan`.
- **`deliverPickResult(multi_peel)`** — update `m_last_peel_hits` from full async peel; skip `applySelection` when promoted front equals primary; apply async front only as **fallback** when sync missed.
- **Keep** left-release pick without prior press (drag/orbit guards unchanged).
- **REMOVED (spec)** — async-first left-click requirement (rejected after QA).
- **RESTORED (spec)** — sync-then-async left-click + static-camera outline/gizmo visibility.
- **Docs** — revert `render-pipeline.md` to sync-then-async wording.

## Capabilities

### New Capabilities

<!-- None -->

### Modified Capabilities

- `viewport-mesh-pick`: Restore sync-then-async left-click; async peel populates/refines cycle list only.
- `viewport-selection-outline`: Outline and gizmo visible on next frame after left-click without camera move (sync path).

## Impact

- `engine/src/runtime/function/editor/viewport_pick_system.cpp` — restore sync pick + peel-list fill on release; remove async-first paths
- `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp` — remove `onAsyncPickFirstPass` hook
- `engine/src/runtime/function/render/overlay/pick_overlay.cpp` — sync APIs used on release (existing)
- `docs/agents/render-pipeline.md` — sync-then-async flow
- Manual QA: `pick_test` left-click `BoxFront` (Hierarchy + outline + gizmo ≤2 frames, static camera); same-pixel cycle; piercing menu regression
