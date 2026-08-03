# Directory Structure

```
Blunder-Engine/            # Engine checkout (NOT a Project — no Assets/Resources)
├── engine/
│   ├── src/
│   │   ├── runtime/       # Core library (engine_runtime)
│   │   └── editor/        # Editor executable (engine_editor)
│   └── shaders/           # Built-in Slang (not project content)
└── engine/3rdparty/       # Git submodules (under engine/)

E:/Blunder Projects/<Name>/  # User Projects (project.blunder + content)
├── Assets/                # Intermediate descriptors (virtual paths: assets/...)
├── Resources/             # Intermediate data bodies (virtual paths: resources/...)
└── .blunder/              # Registry, cook cache, scripts_bin
```

Debug `engine_editor` opens `E:/Blunder Projects/Test` by default (`BLUNDER_DEFAULT_PROJECT_ROOT`).
See [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) for virtual paths, three-tier roles, Pull/Cook,
descriptors, `ContentIndex`, the `.blunder/cache/thumbnails/` thumbnail cache (`ThumbnailGenerator`),
and the Slint **Content Browser** (`ContentBrowserSystem` in `resource/content_browser/`).
Vocabulary: [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline). Decision:
[docs/adr/0012-pull-asset-pipeline.md](../adr/0012-pull-asset-pipeline.md).

**efsw** ([SpartanJ/efsw](https://github.com/SpartanJ/efsw)) is vendored at `engine/3rdparty/efsw`
for cross-platform directory/file change notifications. `ContentBrowserWatch` uses it to
auto-refresh the Content Browser when a Project's `Assets/` or `Resources/` change on disk
(debounced; skips `.blunder/`). Init submodule: `git submodule update --init engine/3rdparty/efsw`.
Defines `BLUNDER_HAS_EFSW` and links `efsw` when the target exists.

## See also

- [design-docs/architecture.md](../design-docs/architecture.md) — five layers ↔ directories
- [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) — on-disk layout and cook
- [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline) — domain vocabulary
- [docs/adr/0012-pull-asset-pipeline.md](../adr/0012-pull-asset-pipeline.md) — Pull pipeline decision
- [cmake.md](cmake.md)
- [overview.md](overview.md)
