# Task 5 Report — Manual checklist (Tasks 5.1, 5.2)

## STATUS

**DOCUMENTED — human verification pending**

Tasks 5.1 and 5.2 are **not** marked complete in `tasks.md`. No manual pass was performed in this agent run (cannot run interactive Content Browser / viewport QA).

## Summary

- Added `openspec/changes/companion-animation-gltf-import/manual-checklist.md` with:
  - **5.1** — Content Browser multi-select Import steps for DogWalk Chocomel host + `LOOP-chocomel-idle` + `LOOP-chocomel-walk`; expected Assets, Resources Intermediate, and Edit/Play stem addressing.
  - **5.2** — Co-located near-disk single-file Import (positive) vs disconnected `animations/world/…` single-file Import (negative / must not attach).
- Left `openspec/changes/companion-animation-gltf-import/tasks.md` items **5.1** and **5.2** as `[ ]` for human sign-off after checklist execution.

## Deliverable

| File | Purpose |
|------|---------|
| `openspec/changes/companion-animation-gltf-import/manual-checklist.md` | Human QA script + pass/fail checkboxes |

## Automated evidence (already on branch; not a substitute for 5.1/5.2)

Integration tests in `asset_import_test.cpp`:

- `importExternalFilesPairsCompanionsIntoMeshImport()` — multi-select host + companions → Mesh + clips, companion Intermediates, no companion Mesh Assets.
- `singleMeshImportDiscoversNearDiskCompanions()` — near-disk attach for co-located layout; excludes disconnected deep tree.

## COMMITS

- `5469c1d` — `Add companion animation manual QA checklist (Tasks 5.1-5.2).`

## Human next steps

1. Run checklist §5.1 and §5.2 in `engine_editor` with Test Project + DogWalk source files.
2. Check `[x]` on `tasks.md` 5.1 / 5.2 only after both sections pass.
3. Task 4.2 (Chocomel import doc for Test Project) remains separate if still open.

## Concerns

- Godot DogWalk repo path is machine-specific (`../../Godot Projects/DogWalk` in workspace); checklist uses repo-relative paths — adjust absolute paths locally.
- §5.2A requires a co-located folder layout (or copied LOOP-shaped companions); not shipped inside Blunder engine repo.
- Does not claim `dogwalk-animation-phase-1` / Phase 2 Done.
