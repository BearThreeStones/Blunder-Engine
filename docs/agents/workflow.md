# Default change path

> Agent map: [AGENTS.md](../../AGENTS.md). Terms: [CONTEXT.md — Agent environment](../../CONTEXT.md#agent-environment-repository). Decision: [ADR 0050](../adr/0050-default-change-path.md).

OpenSpec remains the contract. Superpowers (TDD, debug, verification) and gstack (`/review` only by default) are tools on this path, not a parallel planning system.

## Sequence

1. **Grill** — `grill-with-docs`. Human confirms 3–7 User stories. Required if and only if an OpenSpec change will exist.
2. **Propose** — `/opsx:propose`. Write those User stories into `proposal.md` (scene language, not WHEN/THEN). Draft `manual-checklist.md` 1:1 from the stories. Specs, design, and tasks stay for the agent.
3. **Apply** — `/opsx:apply` (project skills as needed). Working memory is this change folder. Optional `notes.md` only when a rejected approach must outlive chat. No `PLAN.md` dumps, no new `docs/exec-plans/` or `docs/superpowers/plans/` files.
4. **Agent QC** — `/validate` or [common-tasks.md](common-tasks.md#default-validation). Completion evidence is not Human acceptance.
5. **Human acceptance** — the human walks each User story in the windowed editor, or Headless when that story is a no-window path. The agent may draft the checklist; it must not sign.
6. **Adversarial review** — gstack `/review` on the diff. Complexity penalty is a veto here (parallel systems, speculative helpers, a second planning track, a new framework when this path suffices). Disagreement between implementer and review goes to the human.
7. **Archive** — `/opsx:archive`. The archive listing is the Working memory index.

## Change-path stop

No new Agent environment hook. The agent **stops and asks** instead of inventing completion:

- **`/opsx:propose`** — if this conversation has no finished Grill and `proposal.md` would lack User stories, stop. Tell the human to Grill first. Do not skip Grill to keep momentum. Same-conversation Grill may continue into propose.
- **`/opsx:archive`** — if the human has not confirmed they walked the User stories, stop. Do not archive. Do not declare Human acceptance.

Do not add a “Human confirmed” checkbox to the proposal.

## Shortcuts

| Kind | Path |
|------|------|
| Multi-file, optimization, or product-facing | Full sequence above |
| Small bugfix | Systematic debug + smallest diff. No OpenSpec, no Grill, no Human acceptance. Completion evidence still applies to first-party C++. |
| Agent map, skills, or glossary only | Agent-doc maintenance. No OpenSpec, no User stories. |

Do not run `/office-hours`, `/plan-ceo-review`, or `/autoplan` as Default change path phases. `/plan-eng-review` or `/plan-design-review` only when Grill parked a hard-to-reverse trade-off for a second opinion.

## Doc deliverable

Source of truth is versioned Markdown in this repo. GitHub Pages publishes that same source (Docs site) at https://bearthreestones.github.io/Blunder-Engine/. Workflow: `.github/workflows/docs-pages.yml`. Decision: [ADR 0051](../adr/0051-docs-github-pages.md). Do not split a second docs repo, do not adopt OINK/Hugo/MkDocs Material, do not add `llms.txt` in this path. Do not write a tutorial per change unless a User story is first-run.

## See also

- [common-tasks.md](common-tasks.md)
- [MAINTENANCE.md](../MAINTENANCE.md)
- [golden-principles.md](../golden-principles.md) (principle 12)
