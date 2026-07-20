# Architecture (conceptual layers ↔ repository)

The [README](../../README.md) describes five **conceptual** layers (平台 → 工具). The repository uses a **physical** layout (`runtime` / `editor` / `tools`). This document maps them so agents do not confuse README terminology with folder names.

## Conceptual five layers

| Layer (README) | Role |
|----------------|------|
| Platform | OS, windowing, input devices |
| Core | Engine kernel: logging, events, math, memory, application lifecycle |
| Resource | Assets, import, cook, content index, thumbnails |
| Function | Rendering, scene, UI integration, gameplay-facing systems |
| Tool | Editor executable, asset compiler, developer-facing CLIs |

## Physical layout

```
engine/src/
├── runtime/          # engine_runtime library
│   ├── platform/     # SDL window, platform services
│   ├── core/         # log, events, math, memory, Layer/LayerStack
│   ├── resource/     # AssetManager, cook, Content Browser data
│   └── function/     # render, scene, ui, slint, editor helpers, global
├── editor/           # engine_editor executable (thin entry + editor layers)
└── tools/            # asset_compiler, cook-assets target
```

Project content (not engine code):

- `Assets/` — Intermediate descriptors (`assets/…`)
- `Resources/` — Intermediate data bodies (`resources/…`); Source under `Resources/Source/`
- `engine/shaders/` — built-in Slang (engine-owned, not under Assets)

See [docs/agents/structure.md](../agents/structure.md) for the full tree, [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) for virtual paths and cook, [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline) for vocabulary, and [docs/adr/0012-pull-asset-pipeline.md](../adr/0012-pull-asset-pipeline.md) for the Pull decision.

## Layer ↔ directory mapping

| Conceptual layer | Primary locations | CMake target / notes |
|------------------|-------------------|----------------------|
| Platform | `runtime/platform/` (e.g. `window/`) | Part of `engine_runtime` |
| Core | `runtime/core/` (`log/`, `event/`, `math/`, `memory/`, `layer/`) | Part of `engine_runtime` |
| Resource | `runtime/resource/` (`asset/`, `asset_cook/`, `asset_import/`, `thumbnail/`, `content_browser/`) | Part of `engine_runtime` |
| Function | `runtime/function/` — `render/`, `scene/`, `ui/`, `slint/`, `editor/` (runtime-side editor systems), `global/` | Part of `engine_runtime` |
| Tool | `engine/src/editor/` (executable), `engine/src/tools/` (`asset_compiler`) | `engine_editor`, `asset_compiler` |

Third-party code lives under `engine/3rdparty/` (submodules). It is not a README “layer”; treat it as vendored dependencies.

## `Layer` / `LayerStack` is not the five layers

`runtime/core/layer/` implements the **application lifecycle pattern** (ordered `Layer` instances ticked each frame: input, render, UI, etc.). Names like `InputLayer` or `RenderSystem` refer to this pattern, **not** to README’s Platform/Core/Resource/Function/Tool split.

## Editor vs runtime

- **`engine_runtime`** — shared library; most engine logic.
- **`engine_editor`** — executable in `engine/src/editor/`; wires layers, Slint UI, and viewport. Editor-specific systems also appear under `runtime/function/editor/` when shared with runtime.

## Dependency direction (informal)

```
editor  →  engine_runtime  →  3rdparty
tools   →  engine_runtime
```

Avoid `engine_runtime` depending on `engine_editor`. New shared code belongs in `runtime/` unless it is editor-only glue.

## Related docs

- [golden-principles.md](../golden-principles.md) — rules derived from this layout
- [render-pipeline.md](../agents/render-pipeline.md) — viewport data flow
- [coordinate-system.md](../agents/coordinate-system.md) — Z-up / glTF import
