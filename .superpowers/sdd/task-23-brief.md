# Task 6.2 — Smoke path (automated + manual checklist)

Manual smoke: Import FBX → preview Fast Path → Cook Final → edit Source → auto-Reimport → scene GUID mesh reference.

## Pragmatic delivery
1. Add an automated smoke test (or script) covering the non-UI chain with OBJ (Assimp):
   Import OBJ dual-write → loadMesh Fast Path → cookAsset Final → Reimport preserves GUID → scene with mesh GUID reference serializes
2. Document remaining UI-only checklist for human (Content Browser Import FBX, visual preview) in the report — mark those as "human verify" not blockers if automated chain is green
3. Mark 6.2 when automated smoke is green + checklist written

Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-23-report.md
Do not push.
