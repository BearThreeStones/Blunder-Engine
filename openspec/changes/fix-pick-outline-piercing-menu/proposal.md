## Why

`hybrid-gpu-picking` P2 delivers broad-phase multi-hit and removes per-click scene scans, but manual QA on `pick_test.scene.asset` shows two regressions: **left-click selection updates Hierarchy but outline and transform gizmo only appear after moving the camera**, and the **piercing menu panel expands to cover most of the editor window** instead of a compact popup at the cursor.

The outline/gizmo failure matches archived root cause **10.4**: async `applySelection` inside `tickVulkan` runs too late for Slint zero-copy viewport composite when the camera is static. The piercing menu failure is a **Slint layout bug** (`PiercingMenu` is full-window sized; `menu-panel` has no height constraint).

These are presentation-layer fixes on top of the hybrid pick data path — broad phase already returns three promoted rows (verified in editor).

## What Changes

- **Restore sync narrow front-most pick on left release** (input phase): one synchronous 1×1 narrow readback → `promotePickEntity` → `applySelection` before async broad+narrow completes; async delivery refreshes `m_last_peel_hits` only when front matches.
- **Force viewport composite after selection from async path**: ensure `deliverPickResult` and piercing-menu row selection trigger Slint dirty/composite even when `setSelection` is a no-op (same entity re-pick).
- **Fix piercing menu Slint layout**: compact panel sized to row count (`fit-content` height); full-window transparent dismiss layer separate from visible panel; no opaque rectangle covering Inspector/dock.
- **Manual QA** on `pick_test.scene.asset` for tasks previously deferred from `hybrid-gpu-picking` §4.

## Capabilities

### New Capabilities

<!-- None — fixes only -->

### Modified Capabilities

- `viewport-mesh-pick`: Reinstate sync-then-async left-click selection timing for immediate outline/gizmo; broad list still from P2 async path; composite guarantee on static camera.
- `viewport-piercing-menu`: Compact popup layout at cursor; modal dismiss without obscuring unrelated UI panels.

## Impact

- `engine/src/runtime/function/editor/viewport_pick_system.{h,cpp}` — sync narrow front on release; `deliverPickResult` composite/selection guards
- `engine/src/runtime/function/render/render_system.{h,cpp}` — optional `forceViewportComposite()` hook if needed
- `engine/src/runtime/function/slint/piercing_menu.slint` — panel sizing and dismiss layer
- `engine/src/runtime/function/slint/editor_window.slint` — verify overlay z-order unchanged
- `docs/agents/render-pipeline.md` — sync-then-async + broad list wording for left-click
- **Depends on:** `hybrid-gpu-picking` P1/P2 (instance buffer + broad phase); does not revert broad phase
