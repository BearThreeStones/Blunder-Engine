# Agent documentation maintenance

> Keep the harness accurate when code or build layout changes. No automated checks yet — follow this checklist manually.

## When you change…

| Change | Update |
|--------|--------|
| CMake preset names, `binaryDir`, or toolset | [docs/agents/build.md](agents/build.md), [CMakePresets.json](../CMakePresets.json) |
| Editor binary path or VS solution location | [docs/agents/build.md](agents/build.md) |
| Linux build flags or paths | [docs/agents/cursor-cloud.md](agents/cursor-cloud.md) |
| Viewport / render / Slint integration | [docs/agents/render-pipeline.md](agents/render-pipeline.md) |
| Coordinates or glTF import | [docs/agents/coordinate-system.md](agents/coordinate-system.md), [CONTENT_LAYOUT.md](../CONTENT_LAYOUT.md) |
| Assets / Resources / cook pipeline | [CONTENT_LAYOUT.md](../CONTENT_LAYOUT.md), [CONTEXT.md — Asset pipeline](../CONTEXT.md#asset-pipeline), [docs/adr/0012-pull-asset-pipeline.md](adr/0012-pull-asset-pipeline.md), [docs/agents/structure.md](agents/structure.md) |
| New top-level module or layer boundary | [docs/design-docs/architecture.md](design-docs/architecture.md), [docs/golden-principles.md](golden-principles.md) if new invariant |
| Slint fork branch or bump procedure | [docs/agents/slint-fork.md](agents/slint-fork.md) |
| New recurring agent rule | [docs/golden-principles.md](golden-principles.md) (one line + link) |
| Multi-step feature shipped | Move plan from [docs/exec-plans/active/](exec-plans/active/) → [completed/](exec-plans/completed/) |

## AGENTS.md

- Stay a **map** (target ≤ 100 lines): tables and links only, no long prose.
- New topic guides go under `docs/agents/` and get one row in [AGENTS.md](../AGENTS.md).
- Verify relative links resolve after moves.

## Exec plans

- Start in `docs/exec-plans/active/<name>.md` using the [template](exec-plans/README.md).
- On merge or abandon: move to `completed/` with final status, or delete if noise.

## Future mechanical enforcement (not implemented)

Planned for a later harness phase:

- `scripts/check-agent-docs.ps1` — broken links, `AGENTS.md` line count, preset names vs `CMakePresets.json`
- GitHub Actions — `cmake --preset vs2026-debug` build, optional markdown-link-check
- Promote stable [golden-principles.md](golden-principles.md) items into clang-tidy / CTest when tests exist

## See also

- [golden-principles.md](golden-principles.md) — principle 12 (docs as source of truth)
- [design-docs/index.md](design-docs/index.md)
