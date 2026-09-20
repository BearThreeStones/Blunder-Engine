# Design

## Context

See proposal.md for why. Grill locked windowed `engine_editor` only, dismiss when index `refresh` + startup entities + Iterate can run (meshes/textures/thumbs may fly), and a Shell Editor modal overlay with Startup cover unchanged (ADR 0052). Decision: [ADR 0073](../../../docs/adr/0073-editor-open-progress-overlay.md).

Today `startSystems` shows the Startup cover, cooks, brings up Vulkan + Slint, then `presentStartupShell()` dismisses the cover. `SDL_AppInit` then calls `BlunderEngine::initialize()`: windowed editor defers Content Browser `refresh` with `m_content_browser_refresh_pending` and `activateEditorScene` → `openScene` → `loadScene` (JSON + instantiate ~10828 entities on DogWalk; unique Mesh only `MeshLoader::request`). That call returns before `SDL_AppIterate`. First `tickOneFrame` runs the pending `refresh()` scan. Until Iterate, the Shell stays on the first Present — the Godot red-box gap. Mesh Loader already streams after Iterate ([ADR 0072](../../../docs/adr/0072-async-scene-mesh-streaming.md)). Product Project: `E:\Blunder Projects\DogWalk`. Do not edit `cursor/scene-collision-bridge-8a89` / PR 24.

## Goals / Non-Goals

**Goals:**

- One Editor modal overlay on the Shell after cover dismiss; pump + Slint Present during remaining open; dismiss when index scan + entity table + product Iterate.
- Discrete weighted percent (cover stages already credited) + elapsed from first OS window + captions Indexing content / Opening scene.
- Tests without a window. Human walk on DogWalk `se-world` in windowed `engine_editor`.

**Non-Goals (design-level):**

- Percent/elapsed on the Startup cover; second splash HWND; Player/PM/Headless overlay.
- Waiting Mesh Loader / Texture Loader / thumbnail drain.
- Reuse on later Content Browser scene opens.
- Cook / flatten / Mesh Loader / Texture Loader algorithm changes.
- Commits on PR 24.

## Decisions

1. **Overlay is Slint Editor modal on the Shell, after `presentStartupShell`.**  
   Reuse `EditorModalPanel` + `EditorTheme.modal-dim` like Import Mesh (no action row). Native GDI cover cannot show a percent bar (ADR 0052). Overlay mounts once the Shell owns Present.  
   *Alternatives:* keep painting GDI after Shell (fights Skia Present); a second HWND (rejected); percent on the cover (rejected).

2. **Elapsed clock starts at `startupCoverBegin` (first OS window).**  
   Store a monotonic timestamp there; overlay reads whole seconds. Cover still paints only wordmark + stage name.  
   *Alternatives:* restart at overlay show (cannot measure cook); also paint seconds on the cover (Grill default: overlay only).

3. **Fixed integer stage weights; jumps only.**  
   Weights (sum 100): Cooking assets 40, Preparing editor 15, Starting editor 15, Opening scene 20, Indexing content 10. Overlay appears with cover three already complete (70%). Opening scene complete → 90%. Indexing content complete → 100% then dismiss. No wall-time lerp.  
   *Alternatives:* 0% at overlay show (Grill: do not restart cook); smooth crawl (forbidden); equal weights (cook is the long wait).

4. **Captions follow real post-Shell work on `main`.**  
   After Shell Present: `openScene` / `loadScene` instantiate is **Opening scene**; Content Browser `refresh()` scan is **Indexing content**. Today instantiate runs in `initialize()` and scan on first tick — overlay spans both. Thumbnail `tickThumbnailQueue` stays 2/tick after dismiss.  
   *Alternatives:* Godot “global class names” (no such phase); wait thumbs in `refresh` (Grill: thumbs may fly).

5. **Pump is SDL events + Slint Present, not product Iterate.**  
   While overlay is up, poll close/resize/expose and `renderIfNeeded` (same family as `startupCoverPump`, but Shell+modal). Do not treat orbit/click under the dim as product Iterate. Close requests quit and unwinds; no Retry.  
   *Alternatives:* no pump (frozen bar / Not Responding); full `tickOneFrame` as product Iterate before dismiss (overlay would be skippable).

6. **Dismiss gate is index scan + entity table, not Mesh GPU.**  
   `loadScene` may already enqueue Mesh Loader Jobs. Overlay MUST NOT wait `GpuMesh` / Texture Loader / thumbs. Matches Grill default 2 and ADR 0072 first Iterate.  
   *Alternatives:* dismiss at Shell Present (too early); wait 252 unique meshes (fights streaming).

7. **Host gate matches Startup cover, minus Player/PM still uncovered.**  
   Windowed Editor Session only. `startupCoverShouldMount` is the close cousin; overlay additionally requires Shell Slint. Headless / CLI / MCP skip both.  
   *Alternatives:* editor+Player same overlay (Grill: Player splash is another knife).

8. **Session-start only.**  
   Flag the overlay as boot open. `openScene` from Content Browser after dismiss does not re-show it.  
   *Alternatives:* every large scene open (Grill: later knife).

9. **Tests without HWND.**  
   Gating, stage names, weight totals, elapsed origin (null window / fake clock). Windowed paint is Human acceptance, same as `startup_cover_test`.  
   *Alternatives:* require windowed Merge CI (Linux CI is Headless).

10. **Independent of collision-bridge git.**  
    Branch from `main` `3e3d42d`. No files from `cursor/scene-collision-bridge-8a89`.  
    *Alternatives:* stack on PR 24 (forbidden).

## Risks / Trade-offs

- **[Risk] `initialize()` still blocks the UI thread** → Pump + Present during instantiate and `refresh` scan; do not move cook to a worker this slice.
- **[Risk] Overlay flashes on a warm Project** → Allowed (same as cover). No minimum dwell.
- **[Risk] First ticks already run Mesh Loader** → Allowed; dismiss does not wait residency.
- **[Risk] `refresh()` used to stall on scene thumbs** → Overlay-period `refresh` is the index scan + status probe; generating thumbs stays 2/tick after dismiss.
- **[Trade-off] Sparse forest after dismiss** → Allowed; Mesh Loader product.
- **[Trade-off] Title `Opening editor` not Godot `Project initialization`** → Grill default.

## Migration Plan

1. Land planning artifacts (this change). No overlay C++ / Slint yet.
2. On apply: overlay widget + boot pump + discrete percent/elapsed; tests; dismiss gate.
3. Human walk on Windows DogWalk `se-world.scene.asset` in windowed `engine_editor`. Do not merge this planning PR into collision PR 24.
4. Rollback: remove overlay; restore blocking `initialize` without Shell Present pump (cover path stays).

No content format migration.

## Open Questions

None that block specs. Grill defaults locked 2026-09-20. Title `Opening editor`, elapsed overlay-only, no mid-session reuse.
