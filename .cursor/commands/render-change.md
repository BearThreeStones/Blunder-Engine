# Render / viewport change

Use before editing rendering, viewport integration, or Slint composition.

## Read first (required)

1. [docs/agents/render-pipeline.md](docs/agents/render-pipeline.md)
2. [docs/agents/coordinate-system.md](docs/agents/coordinate-system.md)
3. If touching Slint: [docs/agents/slint-fork.md](docs/agents/slint-fork.md)

## Invariants

- Engine renders 3D **offscreen**; Slint + Skia own HWND Present.
- Default path: CPU readback into `SlintSystem::setViewportImage`.
- Z-up world space; glTF converted at import only.

## After implementation

1. Build: `cmake --build build/vs2026-debug --config Debug --target engine_editor`
2. Run `engine_editor` and verify the viewport displays correctly (orbit, pan, resize).
3. Check FPS if performance was the goal; try env toggles from render-pipeline doc.

For performance work, also apply the `viewport-perf` skill.
