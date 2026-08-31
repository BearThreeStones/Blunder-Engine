## Context

See `proposal.md` for why. Today `startSystems` does FileSystem, cook, and most Systems **before** `WindowSystem::initialize`, then `SDL_ShowWindow` immediately. Vulkan device and Slint init run while that HWND is already visible, so the client is empty (black) until Skia first Presents the Editor Shell. Slint shares the engine Vulkan device, so the Shell cannot paint that HWND until `initializeBackend` has run.

`readProjectFile` already yields **Project display name**. Application Bar wordmark is `Blunder Editor - {Project display name}` (`k_editor_title_separator` `" - "` in `active_scene_display`). Headless / CLI / MCP never create this window.

## Goals / Non-Goals

**Goals:**

- Show the session window as soon as Project display name is known, paint the cover on that HWND, finish boot behind it, dismiss on first Shell present.
- Keep the window responsive (close / resize / redraw) while cook and Vulkan still run on the UI thread.

**Non-Goals:**

- Covering until Live scene load or first viewport readback.
- A second splash HWND, SDL_Renderer on the session window, or a Slint UI before the shared device exists.
- Player / Project Manager covers; Linux windowed as a polished path (Merge CI stays Headless).
- Loading Inter before Vulkan; embedding a new font or Logo asset.

## Decisions

### D1 — Reorder windowed Editor boot, do not hide the window
**Choice:** After FileSystem init + `readProjectFile`, create and show the session window, paint the cover, then continue cook / remaining Systems / Vulkan / Slint. Dismiss native cover when the Shell has presented once.
**Why:** Matches grilled “window as soon as we can brand it”; cook is the long wait.
**Rejected:** Show-window-only-after-Slint (no cover); second splash window (ADR 0052); keeping today’s “window after cook”.

### D2 — Native client paint until Slint owns Present
**Choice:** On Windows, paint the client with GDI (or equivalent Win32) using Editor Theme Window `#2A2D31`, light wordmark/stage text, no image. Copy those token values; do not invent a second palette. Typeface is a system UI font (Segoe UI); the Shell’s Inter may replace it at dismiss.
**Why:** Shared Vulkan device is not up yet; SDL_Renderer on the same HWND fights Skia Present.
**Rejected:** Mini Slint splash before device; SDL_Renderer; cinematic black fill.

### D3 — Pump events during remaining `startSystems`, not full ticks
**Choice:** While cover is up, poll SDL events and handle quit / resize / expose so WM_PAINT can redraw. Do **not** run `tickOneFrame` (no 3D, no Slint composite) until Shell init is ready to Present. Close sets quit and unwinds boot without a modal.
**Why:** Story 3 (close) and resize are false if cook blocks `SDL_AppInit` with no pump — Windows shows Not Responding.
**Rejected:** Moving cook to a worker thread this cut (asset/compiler thread-safety); running the full frame loop during boot.

### D4 — Three stage names, call sites not a percent
**Choice:** A small boot-phase enum maps to the exact strings Cooking assets / Preparing editor / Starting editor. Update at the existing phase boundaries (cookIfStale; remaining editor Systems; Vulkan + Slint). Skip a name if that phase is too short to observe.
**Why:** Grilled copy; no fake percent.
**Rejected:** Fine-grained per-file cook messages; a C++ percent of unknown remaining work.

### D5 — Tests without a window
**Choice:** First-party tests cover “cover mounts only for windowed Editor” and stage-name mapping. Windowed paint/close is Human acceptance. If no new test executable stem matches, Agent QC is an `engine_editor` build.
**Why:** Golden principle 9; HWND paint is not Merge CI.
**Rejected:** Requiring a windowed Test run in Merge CI.

## Risks / Trade-offs

- [Font jump Segoe UI → Inter at dismiss] → Accept; no font loader before Vulkan.
- [One-frame black if Present lags after we stop native paint] → Keep native paint until the Shell present notifier/callback, not merely `MainEditorWindow::create()`.
- [Event pump during cook still on UI thread] → Cover stays alive; close can abort. A worker-thread cook is a later change.
- [Linux windowed Debug] → Solid Base 2 fill is enough if that path exists; product acceptance is Windows.
- [Fatal LOG_FATAL during cover] → Process dies as today; last paint may remain until the process is gone.

## Migration Plan

1. Land cover helper + boot reorder + event pump + dismiss on Shell present.
2. Keep CONTEXT **Startup cover** and ADR 0052 with the change (already grilled).
3. Rollback: restore `startSystems` window-after-cook order and remove native paint.

## Open Questions

None.
