# Directory Structure

```
project/
├── Assets/                # Engine asset config (virtual paths: assets/...)
├── Resources/             # Raw source files (virtual paths: resources/...)
├── engine/
│   ├── src/
│   │   ├── runtime/       # Core library (engine_runtime)
│   │   └── editor/        # Editor executable (engine_editor)
│   └── shaders/           # Built-in Slang (not under Assets/)
└── engine/3rdparty/       # Git submodules (under engine/)
```

See [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) for virtual paths, `.mesh.asset` descriptors,
`ContentIndex`, the `.blunder/cache/thumbnails/` thumbnail cache (`ThumbnailGenerator`), and the Slint **Content Browser** (`ContentBrowserSystem` in `resource/content_browser/`).

**efsw** ([SpartanJ/efsw](https://github.com/SpartanJ/efsw)) is vendored at `engine/3rdparty/efsw`
for cross-platform directory/file change notifications. `ContentBrowserWatch` uses it to
auto-refresh the Content Browser when `Assets/` or `Resources/` change on disk (debounced;
skips `.blunder/`). Init submodule: `git submodule update --init engine/3rdparty/efsw`.
Defines `BLUNDER_HAS_EFSW` and links `efsw` when the target exists.

## See also

- [design-docs/architecture.md](../design-docs/architecture.md) — five layers ↔ directories
- [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md)
- [cmake.md](cmake.md)
- [overview.md](overview.md)
