# AGENTS.md — Blunder Engine

> Map for AI agents. Detailed guides live under [`docs/`](docs/). Read links, not everything at once.

## Start here

1. [docs/agents/overview.md](docs/agents/overview.md) — stack, invariants, validation
2. [docs/agents/common-tasks.md](docs/agents/common-tasks.md) — pick docs by task
3. [docs/golden-principles.md](docs/golden-principles.md) — must-follow rules

## Default change path

Spine: Grill → OpenSpec → apply → Agent QC → Human acceptance → Adversarial review → archive.  
Sequence and Change-path stop: [docs/agents/workflow.md](docs/agents/workflow.md). Decision: [ADR 0050](docs/adr/0050-default-change-path.md). Terms: [CONTEXT.md — Agent environment](CONTEXT.md#agent-environment-repository).

| Kind | Path |
|------|------|
| Multi-file or product-facing | Grill (`grill-with-docs`), then `/opsx:propose` … `/opsx:archive` |
| Small bugfix | Debug + smallest diff — no OpenSpec, no Grill, no Human acceptance |
| Agent map / skills / glossary | Agent-doc maintenance — no OpenSpec |

Do not add `docs/exec-plans/` or `docs/superpowers/plans/` files. Do not run `/office-hours`, `/plan-ceo-review`, or `/autoplan` as planning. Default review is gstack `/review` only.

**Doc ownership:** architecture in [docs/design-docs/](docs/design-docs/); Working memory in `openspec/changes/` (index: `openspec/changes/archive/`). Docs site: https://bearthreestones.github.io/Blunder-Engine/ ([ADR 0051](docs/adr/0051-docs-github-pages.md)).

## Getting started

| Topic | Document |
|-------|----------|
| Project overview | [docs/agents/overview.md](docs/agents/overview.md) |
| Task routing | [docs/agents/common-tasks.md](docs/agents/common-tasks.md) |
| AI workflow | [docs/agents/workflow.md](docs/agents/workflow.md), [ADR 0050](docs/adr/0050-default-change-path.md) |
| Build commands | [docs/agents/build.md](docs/agents/build.md) |
| Directory structure | [docs/agents/structure.md](docs/agents/structure.md) |
| Testing | [docs/agents/testing.md](docs/agents/testing.md) |
| Agent environment | [CONTEXT.md](CONTEXT.md#agent-environment-repository), [.cursor/hooks/](.cursor/hooks/) |
| Cursor automation | [.cursor/skills/](.cursor/skills/), [.cursor/commands/](.cursor/commands/), [.cursor/hooks.json](.cursor/hooks.json), [.cursor/mcp.json](.cursor/mcp.json) |

## Development

| Topic | Document |
|-------|----------|
| Code style | [docs/agents/code-style.md](docs/agents/code-style.md) |
| CMake & new systems | [docs/agents/cmake.md](docs/agents/cmake.md) |
| MSVC compiler defines | [docs/agents/msvc-defines.md](docs/agents/msvc-defines.md) |

## Engine

| Topic | Document |
|-------|----------|
| Coordinate system (Z-up, glTF) | [docs/agents/coordinate-system.md](docs/agents/coordinate-system.md) |
| Render data flow & viewport | [docs/agents/render-pipeline.md](docs/agents/render-pipeline.md) |
| Slint fork submodule | [docs/agents/slint-fork.md](docs/agents/slint-fork.md) |

## Design & architecture

| Topic | Document |
|-------|----------|
| Design docs index | [docs/design-docs/index.md](docs/design-docs/index.md) |
| Layers ↔ repository | [docs/design-docs/architecture.md](docs/design-docs/architecture.md) |
| Golden principles | [docs/golden-principles.md](docs/golden-principles.md) |

## Active work

| Topic | Document |
|-------|----------|
| OpenSpec changes (Working memory) | [openspec/changes/](openspec/changes/) |
| Archive index | [openspec/changes/archive/](openspec/changes/archive/) |
| Historical plans (do not add) | [docs/exec-plans/](docs/exec-plans/), [docs/superpowers/plans/](docs/superpowers/plans/) |

## Environments

| Topic | Document |
|-------|----------|
| Cursor Cloud / Linux build | [docs/agents/cursor-cloud.md](docs/agents/cursor-cloud.md) |
| Merge CI (GitHub Actions) | [docs/agents/testing.md](docs/agents/testing.md#merge-ci) |

## References & maintenance

| Topic | Document |
|-------|----------|
| External tool index | [docs/references/index.md](docs/references/index.md) |
| Doc maintenance | [docs/MAINTENANCE.md](docs/MAINTENANCE.md) |

## Related project docs

- [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md) — virtual paths, assets, Content Browser, Pull/Cook
- [CONTEXT.md — Asset pipeline](CONTEXT.md#asset-pipeline) — Source / Intermediate / Final vocabulary
- [docs/adr/0012-pull-asset-pipeline.md](docs/adr/0012-pull-asset-pipeline.md) — Pull pipeline decision
- [Resources/README.md](Resources/README.md) — Resources tree notes
