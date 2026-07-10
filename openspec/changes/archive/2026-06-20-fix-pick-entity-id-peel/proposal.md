## Why

After `fix-pick-depth-peel-reverse-z`, manual QA on `pick_test` still showed a **single-row** piercing menu and no same-pixel click cycling. **Entity-ID peel** fixed Ctrl+right multi-hit. Subsequent revisions added idle poll, poll-before-render, menu input isolation, and selection viewport dirty — fixing **10.5** (menu outline persists ✅) and **10.3** (cycle ✅).

**10.4 still fails:** first left-click updates hierarchy after async `multi_peel` completes, but **outline/gizmo do not appear** without camera movement or a second (sync) click. Same-pixel cycle and piercing menu work because **`applySelection` runs synchronously before `rendererTick`** (input / Slint callback phase).

**Root cause (10.4):** async-only selection on left-click — even with poll-before-render, zero-copy present/composite can lag one or more frames behind selection state. Cycle/menu paths avoid this by syncing selection in the input phase.

**Solution A:** on unmodified left-click release, **sync pick front-most → immediate `applySelection`**, then start async `multi_peel` **only to populate `m_last_peel_hits`** for cycling (do not re-apply selection on async delivery when it matches the sync front hit).

## What Changes

### Already implemented

- Entity-ID peel; `pick_test` layout; docs (§1–3).
- Idle poll; poll-before-render; first-click `multi_peel` request (§4–6).
- Selection viewport dirty; menu input isolation; `hidePiercingMenu` region dirty (§7–8).
- Manual: 10.2, 10.3, 10.5 ✅.

### Added scope (Solution A — sync-then-async)

- **Sync immediate selection on left-click** — `onViewportLeftReleased` (non-cycle path) SHALL call sync `pickEntityAtWindowPosition` / `pickAllAtWindowPosition` front-most (with promote), then **`applySelection` synchronously** before `requestPick(multi_peel)`.
- **Async peel for cycle list only** — when `multi_peel` completes, `deliverPickResult` SHALL update `m_last_peel_hits` / `m_last_click_pixel` and SHALL **not** call `applySelection` if primary selection already matches the promoted front hit (or use dedicated peel-list-only delivery path).
- **Empty sync miss** — if sync pick misses, skip immediate selection; async result may still clear or select per existing rules.
- **Docs** — document sync-then-async left-click flow in `render-pipeline.md`.

**Out of scope:** parent promotion to `Box*`; P2 broad-phase; picking buried box without cycle/menu.

## Capabilities

### Modified Capabilities

- `viewport-mesh-pick`: Sync-then-async left-click; async peel populates cycle list only.
- `viewport-selection-outline`: First left-click outline/gizmo via sync selection (same frame as input).
- `viewport-piercing-menu`: Unchanged (10.5 done).

## Impact

- `engine/src/runtime/function/editor/viewport_pick_system.{h,cpp}` — sync pick on release; peel-list-only async delivery
- `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.{h,cpp}` — optional `PickRequestKind` or delivery flag for peel-list-only
- `docs/agents/render-pipeline.md` — sync-then-async behavior
