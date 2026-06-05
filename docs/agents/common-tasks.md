# Common tasks (doc routing)

> Pick your task, read the linked docs **before** editing code.

| Task | Read first | Also useful |
|------|------------|-------------|
| First time in repo | [overview.md](overview.md), [golden-principles.md](../golden-principles.md) | [structure.md](structure.md), [build.md](build.md) |
| Configure / build (Windows) | [build.md](build.md) | [CMakePresets.json](../../CMakePresets.json), [msvc-defines.md](msvc-defines.md) |
| Build on Linux / Cursor Cloud | [cursor-cloud.md](cursor-cloud.md) | [build.md](build.md) |
| Change viewport / rendering | [render-pipeline.md](render-pipeline.md) | [coordinate-system.md](coordinate-system.md), [slint-fork.md](slint-fork.md) |
| Change coordinates / glTF import | [coordinate-system.md](coordinate-system.md) | [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) |
| Content Browser / assets / cook | [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) | [structure.md](structure.md) |
| Add CMake target or runtime system | [cmake.md](cmake.md) | [structure.md](structure.md), [golden-principles.md](../golden-principles.md) |
| Slint submodule / UI renderer | [slint-fork.md](slint-fork.md) | [render-pipeline.md](render-pipeline.md) |
| New feature (multi-file) | [design-docs/architecture.md](../design-docs/architecture.md) | [exec-plans/README.md](../exec-plans/README.md), [golden-principles.md](../golden-principles.md) |
| Add tests (when introduced) | [testing.md](testing.md) | [cmake.md](cmake.md) |

## Default validation

There is no CI or CTest suite for first-party code yet. After changes:

1. Configure and build per [build.md](build.md) (or [cursor-cloud.md](cursor-cloud.md) on Linux).
2. Run `engine_editor` and exercise the affected UI or scene path.
3. If you touched meshes/textures/import, run `asset_compiler` or re-cook as in [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md).

## See also

- [overview.md](overview.md)
- [golden-principles.md](../golden-principles.md)
- [MAINTENANCE.md](../MAINTENANCE.md)
