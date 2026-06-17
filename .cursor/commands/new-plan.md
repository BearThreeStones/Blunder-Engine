# New execution plan

Create a multi-step execution plan for significant work (3+ subsystems or multiple sessions).

## Steps

1. Read [docs/exec-plans/README.md](docs/exec-plans/README.md).
2. Create `docs/exec-plans/active/<short-name>.md` using this template:

```markdown
# <Title>

**Status:** active
**Started:** YYYY-MM-DD

## Goal

One paragraph: what done looks like.

## Context

- [Link to design doc or issue]
- [golden-principles.md](../../golden-principles.md) items that apply

## Tasks

- [ ] Task 1
- [ ] Task 2

## Validation

- [ ] `cmake --preset vs2026-debug` + build `engine_editor`
- [ ] (feature-specific checks)

## Notes

Decisions and blockers during execution.
```

3. Replace `<short-name>` with a kebab-case slug (e.g. `viewport-perf-opt`).
4. Link relevant docs from [docs/agents/common-tasks.md](docs/agents/common-tasks.md).

When done, move to `docs/exec-plans/completed/` and set **Status:** done.
