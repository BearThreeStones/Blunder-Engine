## Why

`viewport-mesh-pick` shipped dual-path picking (CPU raycast default + optional GPU ID pass). Maintaining two paths adds complexity, and the CPU path does not scale for dense scenes. Meanwhile, overlapping meshes at the same screen pixel cannot be disambiguated with a single front-most pick—Unity solves this with a **Selection Piercing Menu** (Ctrl+right-click lists all hits). Consolidating on GPU picking and adding piercing selection closes both gaps.

## What Changes

- **Remove CPU pick (Scheme A)** entirely: delete `pickCpu`, triangle budget fallback, `BLUNDER_VIEWPORT_GPU_PICK` env gate, and CPU-only helpers used only for picking.
- **GPU ID pick (Scheme B) becomes the only path** for viewport left-click selection (front-most hit at pixel).
- Add **Unity-style Selection Piercing Menu**:
  - **Ctrl+right-click** in viewport → popup listing all pickable entities along the ray at that pixel (front-to-back, deduplicated by entity).
  - Choosing an entry applies selection (replace).
  - **Ctrl+Shift+right-click** → same menu, but choosing an entry **adds** to selection.
- Optional v1 parity: **repeated left-click** on the same viewport pixel cycles through the peeled hit list (front → back) without opening the menu.
- Extend GPU pick to support **multi-hit / depth peel** readback (not just 1×1 front-most sample) for piercing menu and click-cycling.
- Slint UI for the piercing popup at cursor position (entity display names from `SceneInstance`).

## Capabilities

### New Capabilities

- `viewport-piercing-menu`: Unity-style piercing menu and multi-hit GPU pick—depth-peel hit list, Ctrl+right-click popup, modifier semantics, optional same-pixel click cycling.

### Modified Capabilities

- `viewport-mesh-pick`: Supersede dual-path requirements—GPU ID pick is the sole implementation; remove CPU raycast, triangle budget, and env override requirements.

## Impact

- `engine/src/runtime/function/editor/viewport_pick_system.{h,cpp}` — remove CPU path; add piercing menu + peel query API
- `engine/src/runtime/function/render/overlay/pick_overlay.{h,cpp}` — multi-hit depth peel readback
- `engine/src/runtime/function/render/render_system.cpp` — right-click routing for piercing menu
- `engine/src/runtime/function/slint/` — piercing menu popup component + wiring
- `engine/src/runtime/core/math/geometry.h` — remove or retain ray-triangle only if still used elsewhere (audit)
- `docs/agents/render-pipeline.md` — GPU-only pick + piercing menu input table
- **BREAKING (internal):** `BLUNDER_VIEWPORT_GPU_PICK` and CPU pick code paths removed; Ctrl+right-click consumed by piercing menu (not camera/context)
