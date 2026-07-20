# Task 5.5 — Import/Reimport tests gate

Tests: FBX/OBJ Import dual-write; Reimport preserves GUID; glTF Import unchanged success path.

## Scope
Audit asset_import_test coverage. Add FBX dual-write only if a tiny fixture is feasible without proprietary files — OBJ dual-write already covers Assimp path; documenting FBX as whitelist-only with OBJ evidence is OK if adding FBX binary is hard. Ensure Reimport + glTF Intermediate cases green.

Mark 5.5. Report: .superpowers/sdd/task-21-report.md
Do not push.
