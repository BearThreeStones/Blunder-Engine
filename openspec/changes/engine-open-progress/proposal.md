# Proposal

## Why

After the Startup cover yields, windowed `engine_editor` still blocks in `SDL_AppInit` / first ticks (`initialize` → `openScene` / `loadScene` instantiate, Content Browser index `refresh`) with the Shell frozen on the first Present. Opening DogWalk `se-world.scene.asset` (~10828 entities) looks hung; Mesh Loader already streams after Iterate ([ADR 0072](../../../docs/adr/0072-async-scene-mesh-streaming.md)). This change is an **open instrument**, not a second splash and not a Mesh wait. Decision: [ADR 0073](../../../docs/adr/0073-editor-open-progress-overlay.md).

## What Changes

- **Windowed `engine_editor` only.** After the Editor Shell is on screen, show a Slint **Editor modal** overlay (Godot red-box shape: title, discrete percent, stage caption, elapsed seconds). Not Project Manager, not Player, not Headless / CLI / MCP.
- **Dismiss when open can Iterate.** Overlay stays until Content index `refresh` (scan) has finished **and** the startup scene entity table is instantiated **and** product Iterate can run. Mesh Loader Jobs / `GpuMesh`, Texture Loader copies, and thumbnail 2/tick MAY still fly. Overlay MUST NOT wait for those.
- **Startup cover stays ADR 0052.** Same-window brand field, English stage names, **no percent**, no elapsed on the cover, dismisses when the Shell Presents. No second splash HWND. Do not paint a fake percent on the cover.
- **Elapsed from the first OS window.** Overlay seconds start at `startupCoverBegin`, not at overlay show. Cover stages (Cooking assets / Preparing editor / Starting editor) count as the first half of open **time**; overlay percent MUST NOT restart those stages at 0%.
- **Captions map real Blunder boot.** Overlay subtitles: **Indexing content** (Content Browser `refresh` scan), **Opening scene** (`loadScene` deserialize + instantiate). No Godot “global class names”. Percent jumps at discrete weighted stage boundaries. No smooth fake crawl.
- **Session-start only.** This overlay is the **this-session startup open**. Opening another scene from the Content Browser later does not show it. Close while it is up ends the Editor Session; no Cancel / Retry.
- **Instrument, not accelerator.** Do not change cook, flatten, Mesh Loader budget, or Texture Loader.

**Out of scope:** Project Manager chrome bar; Player / game-shell splash; Headless / CLI / MCP overlay; percent on the Startup cover; second splash HWND; waiting Mesh / texture / thumbnail queues; Content Browser dock scan bar (Godot lower-left); later “open another big scene” reuse; Chinese localization; Mesh Loader / Texture Loader / flatten / cook edits; `cursor/scene-collision-bridge-8a89` / [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24).

## User stories

1. Open DogWalk from Project Manager or Debug. After windowed `engine_editor` appears, and before the window can Iterate, a Godot red-box style center progress is visible: title, percent bar, current stage caption, and elapsed seconds.
2. The Shell (Application Bar, docks, Viewport grid) may already be on screen. Progress is a modal overlay on that Shell, not a second splash window, and not the Startup cover held until the Live scene finishes.
3. After the entity table is built and the window can Iterate, the overlay is gone. The forest may still grow (Mesh Loader); checkerboard albedo (Texture Loader) may still fly; Content Browser thumbnails may still be 2/tick. Those are not overlay leftover.
4. Closing the window while the overlay is up ends this Editor Session. There is no Retry.
5. Headless, `--mcp`, CLI, Project Manager, and Player have no overlay.
6. The open target is the DogWalk main scene `se-world.scene.asset`. Test and Sponza are not the demo Project. The dog does not need to walk. Collision wireframe and C# Find are not required.

## Capabilities

### New Capabilities

- `engine-open-progress`: Windowed `engine_editor` Slint Editor modal overlay after Startup cover dismiss until Content index refresh + startup entities + product Iterate. Title `Opening editor`. Discrete weighted percent (cover stages already credited). Elapsed whole seconds from first OS window. Captions Indexing content / Opening scene. Pump + redraw so the bar is not a frozen Present. Close ends the session. No Cancel/Retry. Not Headless / CLI / MCP / Player / Project Manager. Not a Mesh / texture / thumbnail wait. Not a second splash.
- `dogwalk-engine-open-progress`: Human walk on DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor`. Overlay visible after cover, gone when Iterate can run; forest/thumbs/textures may still fly. Not Test/Sponza. Not collision QC `root.scene.asset`. Not PR 24.

### Modified Capabilities

- `startup-cover`: Cover still dismisses when the Editor Shell is on screen and still shows no percent. It SHALL NOT show elapsed seconds. It SHALL NOT wait for Content index refresh or startup scene instantiate (those belong to `engine-open-progress`).

## Impact

- **Engine:** After `presentStartupShell`, show the overlay on the Shell; pump SDL + Slint Present during remaining `startSystems` tail / `BlunderEngine::initialize` / Content index `refresh` so the window is not Not Responding and the bar is not a frozen texture. Dismiss before product Iterate. Mesh Loader enqueue from `loadScene` may already be in flight; overlay does not wait it.
- **UI:** New authored Editor modal in `editor_window.slint` (existing `EditorModalPanel` + dim). English. No action buttons.
- **Tests:** Host gating, stage names, discrete percent/elapsed mapping without a window (`engine_open_progress_test` or equivalent). Windowed feel is Human acceptance.
- **Docs:** CONTEXT **Editor open progress**; [ADR 0073](../../../docs/adr/0073-editor-open-progress-overlay.md). Cover glossary stays ADR 0052.
- **This planning change** is OpenSpec artifacts + glossary/ADR only — no engine/Slint C++ until `/opsx:apply`.
