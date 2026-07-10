## Context

`hybrid-gpu-picking` P2 replaced iterative depth peel with GPU broad phase + candidate narrow. Manual QA on `pick_test.scene.asset` (2026-07-01) confirms:

| Check | Result |
|-------|--------|
| Ctrl+right piercing menu rows | ✅ 3 rows (`BoxFront`, `BoxMid`, `BoxBack`) |
| Hierarchy on left-click | ✅ Updates |
| Outline + transform gizmo (static camera) | ❌ Only after camera move |
| Piercing menu panel size | ❌ Full-window opaque rectangle |

**Outline/gizmo:** P2 removed sync-then-async on left release. Selection now runs in `deliverPickResult` during `tickVulkan` poll — same failure mode as archived **10.4** (`2026-06-20-fix-pick-entity-id-peel`, `fix-viewport-left-click-async-pick`). Hierarchy updates via `applySelection` but Slint zero-copy viewport composite defers outline/gizmo until `camera_changed` forces a full render.

**Piercing menu:** `editor_window.slint` sizes `PiercingMenu` to `parent.width/height` (full window). `piercing_menu.slint` `menu-panel` has `min-width`/`max-width` but no height; inner `TouchArea { height: 100% }` stretches the panel background to fill the parent — a huge dark rectangle over Inspector/docks.

**Existing sync APIs:** `RenderSystem::pickEntityAtWindowPosition` and `pickAllEntitiesAtWindowPosition` remain (full-scene narrow via `PickOverlay`). Broad-phase async path is unchanged.

## Goals / Non-Goals

**Goals:**

- Left-click on `pick_test`: Hierarchy + outline + gizmo within ≤2 frames **without moving camera**.
- Same-pixel repeat click cycles `BoxFront` → `BoxMid` → `BoxBack` (second click works even if async list still in flight).
- Piercing menu: compact popup at cursor; transparent dismiss outside panel; ≥3 rows still listed.
- Async broad list still refines `m_last_peel_hits`; no regression to pre-P2 broad phase.

**Non-Goals:**

- Slint composite scheduler rewrite (archived 10.4: async-only dirty insufficient).
- Reverting P1 instance buffer or P2 broad compute.
- Continuing `fix-viewport-left-click-async-pick` as a separate change (superseded by this fix on hybrid stack).

## Decisions

### 1. Restore sync-then-async on left release (not async-only)

**Decision:** On `onViewportLeftReleased` when issuing new `multi_peel` (not local cycle branch):

1. **Sync front-most** — `pickEntityAtWindowPosition` → `promotePickEntity`.
2. If valid → **`applySelection` in input phase** (before next `tickVulkan`).
3. **Sync peel list** — `pickAllEntitiesAtWindowPosition` → `dedupePromotedPickHits` → assign **`m_last_peel_hits`** so second click can cycle immediately.
4. **`requestPick(multi_peel)`** — async broad + narrow as today.
5. **`deliverPickResult(multi_peel)`** — refresh `m_last_peel_hits` from async; **skip `applySelection`** if promoted front equals `getPrimarySelection()`; if sync missed but async has hits → apply async front; always `requestViewportRedraw()` when list updates.

6. **Defer async pick one frame** — after sync `applySelection`, schedule `requestPick(multi_peel)` on the **next** `tickVulkan` (not the click frame). Lets the selection frame render outline/gizmo and composite before broad-phase GPU work contends with the offscreen pass.

7. **Force Skia composite on viewport pick** — `notifyEditorUi` calls `markFullSkiaRefresh()`; `endFrame` must not throttle composite when that flag is set (SDL path unlike Slint hierarchy clicks).

**Rationale:** Matches archived 10.4 fix and Hierarchy/piercing-row timing. Sync uses existing `PickOverlay` narrow (acceptable cost for one 1×1 click); async broad phase still avoids per-click full-scene scans for multi-hit.

**Alternatives considered:**

- **Force Slint composite only** — insufficient per archive; Hierarchy already updates, outline does not.
- **Re-select same entity to bust `setSelection` early-return** — hacky; does not fix first-click timing.

### 2. Piercing menu: split dismiss layer from panel

**Decision:** Restructure `piercing_menu.slint`:

- Root `Rectangle` stays full-window for dismiss `TouchArea` (transparent, no background).
- `menu-panel` gets explicit height from content: `padding + rows * row_height` (or `VerticalLayout` intrinsic height); **no** `height: 100%` on panel or panel `TouchArea`.
- Keep anchor at cursor; clamp to window bounds if needed (follow-up if overflow).

**Rationale:** Full-window dismiss is correct for modal behavior; only the visible panel must be compact.

### 3. Composite after async delivery when selection unchanged

**Decision:** When `deliverPickResult` skips `applySelection` (front matches), still call `requestViewportRedraw()` if peel list changed. Cycle branch already calls `applySelection` (different entity).

**Rationale:** Covers edge case where list refresh should not thrash selection but viewport may need peel pass progress.

## Risks / Trade-offs

- **[Risk] Sync narrow on every left-click reintroduces GPU stall** → Mitigation: single 1×1 immediate command; same as pre-P2; broad phase still async for multi-hit; acceptable for editor UX.
- **[Risk] Sync and async front disagree** → Mitigation: async fallback applies when sync misses; when both hit, async refines list only.
- **[Risk] Slint height calc wrong for empty/huge lists** → Mitigation: cap visible rows or max-height + scroll in follow-up; P1 uses ≤1024 broad hits but menus typically &lt;20 rows.

## Migration Plan

1. Implement sync-then-async in `viewport_pick_system.cpp`.
2. Fix `piercing_menu.slint` layout; verify `editor_window.slint` z-order unchanged.
3. Update `docs/agents/render-pipeline.md` sync-then-async + broad list wording.
4. Manual QA `pick_test.scene.asset`; then archive this change and update `hybrid-gpu-picking` task 4.x honestly.

## Open Questions

- None blocking — sync API and Slint layout approach are proven from prior changes.
