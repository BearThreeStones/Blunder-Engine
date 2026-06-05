# Execution plans

> Short-lived plans for multi-step agent or human work. Keeps task context in the repo instead of only in chat.

## Layout

| Folder | Purpose |
|--------|---------|
| [active/](active/) | Work in progress — one file per initiative |
| [completed/](completed/) | Shipped or abandoned plans (archive) |

## When to create a plan

- Touches 3+ subsystems or many files
- Spans multiple sessions or agents
- Needs explicit validation steps beyond “build + run”

Skip for single-file fixes; use [common-tasks.md](../agents/common-tasks.md) routing instead.

## Template

Copy into `active/<short-name>.md`:

```markdown
# <Title>

**Status:** active | blocked | done  
**Owner:** (optional)  
**Started:** YYYY-MM-DD

## Goal

One paragraph: what done looks like.

## Context

- [Link to design doc or issue]
- [golden-principles.md](../golden-principles.md) items that apply

## Tasks

- [ ] Task 1
- [ ] Task 2

## Validation

- [ ] `cmake --preset vs2026-debug` + build `engine_editor`
- [ ] (feature-specific checks)

## Notes

Decisions and blockers during execution.
```

## Lifecycle

1. Create under `active/` when starting significant work.
2. Update tasks and notes as you go.
3. On merge: set **Status:** done, move file to `completed/`.
4. On cancel: move to `completed/` with reason, or delete if no lasting value.

See [MAINTENANCE.md](../MAINTENANCE.md) for doc sync rules.

## See also

- [common-tasks.md](../agents/common-tasks.md)
- [design-docs/architecture.md](../design-docs/architecture.md)
