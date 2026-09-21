# Editor open progress is a Shell modal, not a second splash

Windowed `engine_editor` still looks hung after the Startup cover yields: `initialize` / first ticks block on Content index `refresh` and startup `loadScene` instantiate while the Shell is frozen on the first Present. We decided on a **Slint Editor modal overlay** on that Shell (Godot red-box shape: title, discrete percent, stage caption, elapsed seconds from the first OS window). It dismisses when the session can Iterate (index scan + entity table). It does **not** wait for Mesh Loader / Texture Loader / thumbnails. The Startup cover stays [ADR 0052](0052-startup-cover-same-window.md): same window, no percent, gone at Shell Present. Domain: [CONTEXT.md — Editor open progress](../../CONTEXT.md).

**Status:** proposed

## Considered Options

- **Keep the frozen first Shell Present with no overlay** — rejected. Grill: the open gap after cover is unreadable; this knife is the instrument.
- **Hold the Startup cover until Live scene / Iterate** — rejected. ADR 0052: cover ends when the Shell is on screen; it must not wait for the Live scene.
- **Paint a percent (or elapsed) on the Startup cover** — rejected. ADR 0052 forbids a percent on that surface; Grill: elapsed lives on the overlay.
- **Second splash HWND (Unity/Unreal style)** — rejected. ADR 0052 already rejected two taskbar entries.
- **Wait until 252 unique Mesh `GpuMesh` uploads finish** — rejected. Fights Mesh Loader first Iterate ([ADR 0072](0072-async-scene-mesh-streaming.md)). Overlay dismisses while meshes / textures / thumbs may still fly.
- **Dismiss as soon as Shell Presents** — rejected. That flash cannot measure `initialize` / index-scan freeze.
- **Same overlay on Project Manager, Player, Headless / CLI / MCP** — rejected. Grill lock: windowed `engine_editor` only. Player splash is a later knife.
- **Reuse the overlay when opening another scene from the Content Browser** — rejected this slice. Session-start open only.
- **Stack this planning git on `cursor/scene-collision-bridge-8a89` / PR 24** — rejected. Branch from `main`.
