## Context

### Prior work

| Change | Status |
|--------|--------|
| `fix-pick-promote-scene-root` | `promotePickEntity` walk-up to scene-root child; unit tests pass |
| `fix-viewport-left-click-async-pick` (async-first, **implemented**) | QA failed — see below |
| Archived `2026-06-20-fix-pick-entity-id-peel` | Sync-then-async fixed 10.4 (outline/gizmo without camera move) |

### Async-first QA (2026-07-01, `pick_test.scene.asset`)

| Path | Result |
|------|--------|
| Hierarchy click `BoxFront` | Works |
| Ctrl+right piercing menu | 3 rows |
| Viewport left-click → Hierarchy | ✅ Highlights on first click |
| Viewport left-click → outline + gizmo (static camera) | ❌ Requires camera move |
| Same-pixel left-click cycle | ❌ No cycle (Hierarchy stays on first hit) |

### Root cause (two independent failures)

```
输入阶段 applySelection          tickVulkan 内 applySelection
(Hierarchy / 穿透菜单行 / 循环)   (onAsyncPickFirstPass)
        │                              │
        ▼                              ▼
 markViewportDirtyRegion         Slint 本帧合成已决策
 onSelectionChanged              idle 档不立刻 composite
        │                              │
        ▼                              ▼
 描边/gizmo 下一帧可见 ✅          要动相机才看见 ❌
```

**Cycle failure:** `onViewportLeftReleased` cycle branch requires `m_last_peel_hits.size() > 1`. Async-first only fills the list when `deliverPickResult` completes (multi-frame). Second click often arrives with `size ≤ 1` → falls through to `requestPick` → `cancelInFlight()` → peel never finishes with ≥3 hits.

Sync-then-async fills `m_last_peel_hits` **on release** (same timing as piercing menu row click path).

## Goals / Non-Goals

**Goals:**

- Left-click on `pick_test` selects scene-root child with **Hierarchy + outline + gizmo** within ≤2 frames **without moving camera**.
- Same-pixel repeat click cycles `BoxFront` → `BoxMid` → `BoxBack` (Hierarchy updates on second click even if viewport composite is slow).
- Async `multi_peel` refines `m_last_peel_hits` without redundant `applySelection` when sync front matches.
- Keep press/release relaxation when `onViewportLeftPressed` was skipped.

**Non-Goals:**

- Pure async-first left-click ( **rejected** ).
- Slint composite scheduling rewrite (archived 10.4: async-only dirty region insufficient).
- P1/P2 hybrid pick broad phase.

## Decisions

### 1. REJECTED: Async-only left-click peel

**Was:** Remove sync GPU on release; `onAsyncPickFirstPass` in `poll`.

**Rejected because:** QA confirms Hierarchy updates but outline/gizmo lag and cycle broken. Reintroduces archived 10.4.

### 2. RESTORED: Sync-then-async left-click (Solution A)

**Decision:** On `onViewportLeftReleased` when issuing a new `multi_peel` (not local cycle branch):

1. **Sync front-most** — `pickEntityAtWindowPosition` or first hit of `pickAllEntitiesAtWindowPosition`, then `promotePickEntity`.
2. If valid → **`applySelection` immediately** (input phase, before next `tickVulkan`).
3. **Sync peel list** — `pickAllEntitiesAtWindowPosition` (or sync peel loop) → `dedupePromotedPickHits` → assign **`m_last_peel_hits`** so second click can cycle without waiting for async.
4. **`requestPick(multi_peel)`** — async entity-ID peel (1 pass/frame) as today.
5. **`deliverPickResult(multi_peel)`** — refresh `m_last_peel_hits`; skip `applySelection` if promoted front equals `getPrimarySelection()`; if sync missed but async hits, apply async front.

**Rationale:** Matches Hierarchy click and piercing menu row timing. Archived design + QA evidence.

**Alternatives considered:**

- **Async-first + fix Slint only** — rejected in archive; insufficient for 10.4.
- **Sync selection only, async list only** — still leaves outline lag on first click.

### 3. REMOVE: `onAsyncPickFirstPass` applySelection

**Decision:** Remove the hook from `HybridGpuPickSystem::poll` and `ViewportPickSystem::onAsyncPickFirstPass` (or make it list-only telemetry behind debug). Selection must not run inside `tickVulkan` for left-click.

### 4. KEPT: Press/release relaxation

**Decision:** Left release inside viewport without prior `onViewportLeftPressed` still runs pick when drag/orbit guards pass.

### 5. KEPT: `deliverPickResult` multi_peel tail semantics

**Decision:** Final delivery updates `m_last_peel_hits`; guards double `applySelection` when sync already selected front.

### 6. Spec pivot

**Decision:** **REMOVE** async-first requirement; **RESTORE** sync-then-async; **ADD/MODIFY** viewport-selection-outline static-camera scenarios.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Double GPU work (sync readback + async peel) | Sync is one 1×1 readback + bounded sync peel for list; acceptable for editor click |
| Sync miss at pixel | Async delivery applies front hit as fallback |
| `setSelection` early return same entity | Sync + async same front → no redundant redraw |
| Sync peel list cost on release | Cap sync peel iterations (same as `k_max_peel_count`); async refines if needed |

## Migration Plan

1. Revert async-first code paths (`onAsyncPickFirstPass` selection, async-only release).
2. Restore sync pick + sync `m_last_peel_hits` fill + `applySelection` on release.
3. Keep press/release relaxation and debug logs (`BLUNDER_PICK_DEBUG=1`).
4. Update `render-pipeline.md` to sync-then-async.
5. Manual QA 5.2–5.4; archive when pass.

## Open Questions

- Sync list API: reuse `pickAllEntitiesAtWindowPosition` vs inline sync peel loop? **Default: pickAll + dedupePromotedPickHits** (consistent with peel semantics).
- If sync peel returns 1 hit but async later finds 3, cycle before async completes uses sync list; async delivery overwrites `m_last_peel_hits` — acceptable.
