# collada-intermediate SDD ledger

Branch: feat/asset-pipeline-pull
Worktree: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Model: cursor-grok-4.5-high
TDD: mandatory (RED before production code)
Do not push.

## Global constraints
- Mesh Intermediate = COLLADA (.dae); Final = .blunder/cooked binary
- Source Export whitelist: FBX, OBJ, glTF, GLB ¡ú .dae + Source archive
- Intermediate-direct: .dae + images; .blend reject
- texture_guids authoritative; no GUID in COLLADA extras
- Fail-soft Intermediate Upgrade (later tasks)
- Keep changes minimal; follow existing AssetImportService patterns

Task 1.1: complete (c1090fc..345d1f3, review Approved)
Task 1.2: complete (`7d7518a`, Assimp Source Export ¡ú COLLADA .dae)
Task 1.2: complete (345d1f3..7d7518a, review Approved)
Task 1.3: complete (`5746df8`, evidence-only; `asset_import_test` green)
Task 1.3: complete (7d7518a..5746df8, review Approved)
Task 2.1: complete (5746df8..1b5b24d, review Approved)
Task 2.2: complete (`106df69`, cook/watch fixtures ¡ú Intermediate `.dae`)
Task 2.2: complete (pending review; 106df692f695ab47501506792f4e1d84345f3dc7)
Task 2.2: complete (1b5b24d..106df69, review Approved)
Task 2.3: complete (`6b8f0ee`, Fast Path + Intermediate `.dae` invalidation tests)
Task 2.3: complete (106df69..6b8f0ee, review Approved)
Task 3.1: complete (lazy Intermediate Upgrade success path; see task-ci-7-report.md)
Task 3.1: complete (6b8f0ee..0704e8e, review Approved)
Task 3.2: complete (0704e8e..7518df8, review Approved)
Task 3.3: complete (a657fc8, upgrade/reimport test gate; see task-ci-9-report.md)

Task 3.3: complete (7518df8..a657fc8, review Approved)
Task 4.1: complete (83f9886..9467f2d, review Approved)
Task 4.2: complete (a3dcdcc, all 12 targeted binaries exit 0)
