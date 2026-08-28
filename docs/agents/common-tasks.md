# Common tasks (doc routing)

> Pick your task, read the linked docs **before** editing code.

| Task | Read first | Also useful |
|------|------------|-------------|
| First time in repo | [overview.md](overview.md), [golden-principles.md](../golden-principles.md) | [structure.md](structure.md), [build.md](build.md) |
| Configure / build (Windows) | [build.md](build.md) | [CMakePresets.json](../../CMakePresets.json), [msvc-defines.md](msvc-defines.md) |
| Build on Linux / Cursor Cloud | [cursor-cloud.md](cursor-cloud.md) | [build.md](build.md) |
| Change viewport / rendering | [render-pipeline.md](render-pipeline.md) | [coordinate-system.md](coordinate-system.md), [slint-fork.md](slint-fork.md) |
| Change coordinates / glTF import | [coordinate-system.md](coordinate-system.md) | [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) |
| Content Browser / assets / cook | [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) | [structure.md](structure.md), [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline), [ADR 0012](../adr/0012-pull-asset-pipeline.md) |
| Add CMake target or runtime system | [cmake.md](cmake.md) | [structure.md](structure.md), [golden-principles.md](../golden-principles.md) |
| Slint submodule / UI renderer | [slint-fork.md](slint-fork.md) | [render-pipeline.md](render-pipeline.md) |
| New feature (multi-file) | [design-docs/architecture.md](../design-docs/architecture.md) | [exec-plans/README.md](../exec-plans/README.md), [AGENTS.md workflow routing](../../AGENTS.md#workflow-routing-openspec--superpowers--gstack), [golden-principles.md](../golden-principles.md) |
| Add tests | [testing.md](testing.md) | [cmake.md](cmake.md) |
| Merge CI (GitHub Actions Linux) | [testing.md](testing.md#merge-ci) | [cursor-cloud.md](cursor-cloud.md) |

## Default validation

After first-party engine changes:

1. Configure and build per [build.md](build.md) (or [cursor-cloud.md](cursor-cloud.md) on Linux).
2. Run relevant tests under `engine/src/tests/` — see [testing.md](testing.md). Prefer `ctest --test-dir <build> -C Debug -R <stem> --output-on-failure`. Compiling a `*_test` target is not a Test run. If no test name contains a distinctive stem from the edited basename, an observed `engine_editor` build is the documented fallback.
3. If you touched meshes/textures/import, run `asset_compiler` or re-cook as in [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md).

The Completion gate records successful Shell commands from this session; claims in chat are not evidence. See [CONTEXT.md — Agent environment](../../CONTEXT.md#agent-environment-repository). Enable **Cursor Settings → Hooks** if the gate does not fire.

To turn a confirmed escaped defect into a test, invoke `/promote` (Promotion arming). That session also needs Promotion evidence: edit a test under `engine/src/tests/`, observe a failing Test run, then a passing Test run of the same test name. Arming does not waive Completion evidence.

## See also

- [overview.md](overview.md)
- [golden-principles.md](../golden-principles.md)
- [MAINTENANCE.md](../MAINTENANCE.md)
