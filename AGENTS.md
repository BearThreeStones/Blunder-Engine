# AGENTS.md — Blunder Engine

> Map for AI agents. Detailed guides live under [`docs/`](docs/). Read links, not everything at once.

## Start here

1. [docs/agents/overview.md](docs/agents/overview.md) — stack, invariants, validation
2. [docs/agents/common-tasks.md](docs/agents/common-tasks.md) — pick docs by task
3. [docs/golden-principles.md](docs/golden-principles.md) — must-follow rules

## Workflow routing (OpenSpec + Superpowers + gstack)

Three layers — pick one primary tool per phase; do not run duplicate planning flows.

| Layer | Tool | Role | Location |
|-------|------|------|----------|
| Spec | **OpenSpec** | What to build — proposal, design, tasks, archive | `openspec/`, `/opsx:*` in [.cursor/commands/](.cursor/commands/) |
| Process | **Superpowers** | How to work — TDD, debugging, verification, parallel tasks | Cursor plugin (enabled in [.cursor/settings.json](.cursor/settings.json)) |
| Review | **gstack** | Quality gates — role reviews, code review, QA, ship | Global `~/.cursor/skills/gstack-*` |

**Default flow (multi-file feature or optimization):**

1. **Propose** — `/opsx:propose "<kebab-name>"` → `openspec/changes/<name>/`
2. **Implement** — `/opsx:apply` (or follow `tasks.md`); use project skills ([blunder-engine](.cursor/skills/blunder-engine/), [viewport-perf](.cursor/skills/viewport-perf/) when relevant)
3. **Verify** — `/validate` or [common-tasks validation](docs/agents/common-tasks.md#default-validation); Superpowers requires evidence before claiming done
4. **Review** — gstack `/review` on the diff
5. **Archive** — `/opsx:archive`; optional gstack `/ship` for PR

**When to add gstack planning** (skip for narrow technical fixes):

| Situation | gstack skill |
|-----------|--------------|
| Product direction unclear | `/office-hours` |
| Scope or strategy challenge | `/plan-ceo-review` |
| Architecture / eng trade-offs | `/plan-eng-review` |
| UI / UX decisions | `/plan-design-review` |
| Staging URL QA | `/qa` |
| Web research | `/browse` (not Chrome MCP) |

**When to lean on Superpowers** (usually automatic):

| Situation | Superpowers skill |
|-----------|-------------------|
| Ambiguous requirements before propose | `brainstorming` |
| Bug or unexpected behavior | `systematic-debugging` |
| New behavior with tests | `test-driven-development` |
| Independent parallel subtasks | `dispatching-parallel-agents` |
| Before “done” / commit / PR | `verification-before-completion` |

**Task-type shortcuts:**

| Task type | Primary path | Usually skip |
|-----------|--------------|--------------|
| Technical optimization (e.g. viewport FPS) | OpenSpec propose → apply → archive | gstack `/office-hours`, `/plan-ceo-review` |
| New product-facing feature | gstack `/office-hours` → OpenSpec propose | gstack `/autoplan` (overlaps OpenSpec) |
| Small bugfix | Superpowers debugging → minimal diff | OpenSpec change folder |
| Large milestone | `docs/exec-plans/` **or** OpenSpec — not both for the same work |

**Doc ownership:** long-lived architecture stays in [docs/design-docs/](docs/design-docs/); active changes live in `openspec/changes/`; [docs/exec-plans/](docs/exec-plans/) for repo-wide milestones.

## Getting started

| Topic | Document |
|-------|----------|
| Project overview | [docs/agents/overview.md](docs/agents/overview.md) |
| Task routing | [docs/agents/common-tasks.md](docs/agents/common-tasks.md) |
| AI workflow (OpenSpec / Superpowers / gstack) | [Workflow routing](#workflow-routing-openspec--superpowers--gstack) below |
| Build commands | [docs/agents/build.md](docs/agents/build.md) |
| Directory structure | [docs/agents/structure.md](docs/agents/structure.md) |
| Testing | [docs/agents/testing.md](docs/agents/testing.md) |
| Cursor automation | [.cursor/skills/](.cursor/skills/), [.cursor/commands/](.cursor/commands/), [.cursor/mcp.json](.cursor/mcp.json) |

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
| Exec plan template & usage | [docs/exec-plans/README.md](docs/exec-plans/README.md) |
| In-progress plans | [docs/exec-plans/active/](docs/exec-plans/active/) |
| Completed plans | [docs/exec-plans/completed/](docs/exec-plans/completed/) |

## Environments

| Topic | Document |
|-------|----------|
| Cursor Cloud / Linux build | [docs/agents/cursor-cloud.md](docs/agents/cursor-cloud.md) |

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
