# New change (Default change path)

Do **not** create `docs/exec-plans/` files. Working memory is an OpenSpec change. See [docs/agents/workflow.md](docs/agents/workflow.md) and [ADR 0050](docs/adr/0050-default-change-path.md).

## Steps

1. **Small bugfix** — stop here. No OpenSpec, no Grill. Debug and make the smallest diff.
2. **Agent-doc maintenance** (map, skills, glossary only; no engine behavior) — stop here. No OpenSpec. Edit the docs/skills.
3. **Otherwise** this needs an OpenSpec change. **Grill first** (`grill-with-docs`). Do not `/opsx:propose` until the human confirmed 3–7 User stories.
4. Then `/opsx:propose "<kebab-name>"`.
