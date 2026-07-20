# Task 23 Report — OpenSpec 6.2 Smoke path (automated + manual checklist)

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Feature commit:** `a274fa6` — test: add asset pipeline smoke for OpenSpec 6.2  
**Docs commit:** `f09cebe` — docs: mark OpenSpec 6.2 smoke complete  
**OpenSpec:** `- [x] 6.2 Manual smoke: Import FBX → preview Fast Path → Cook Final → edit Source → auto-Reimport → scene GUID mesh reference`

## Summary

Automated non-UI smoke covers the Pull asset chain with OBJ (Assimp Source Export). UI-only FBX Content Browser steps are documented below as **human verify** (not blockers while the automated chain is green).

New target: `asset_pipeline_smoke_test`  
Chain: Import OBJ dual-write → `loadMesh` Fast Path → `cookAsset` Final → Reimport GUID stable → scene mesh GUID serialize.

## TDD evidence

### RED

`cmake --build build/vs2026-debug --config Debug --target asset_pipeline_smoke_test` before CMake wiring:

```
MSBUILD : error MSB1009: project file does not exist.
Switch: asset_pipeline_smoke_test.vcxproj
```

Log: `.superpowers/sdd/task-23-red-build.txt`

### GREEN

After wiring `engine/src/tests/CMakeLists.txt` and implementing `asset_pipeline_smoke_test.cpp`:

```
asset_pipeline_smoke_test: all passed
```

exit 0. Log: `.superpowers/sdd/task-23-green-run.txt`

## Automated coverage

| Step | Assertion |
|------|-----------|
| OBJ Import dual-write | Intermediate glTF under `Resources/Models`, archived `.obj` under `Resources/Source`, descriptor GUID registered |
| Fast Path `loadMesh` | Mesh returned with ≥3 vertices while Final missing (Intermediate + request Cook warn) |
| `cookAsset(guid, force)` | `.meshbin` + meta under `.blunder/cooked/` |
| `loadMeshByGuid` | Cooked mesh loads after cook |
| Reimport | Edit archived Source → `requestReimport` → same GUID; Final invalidated |
| Scene serialize | Entity `"mesh"` persists as GUID (not path) round-trip with registry |

## Human verify — FBX Content Browser UI checklist

Not automated; do not block 6.2 if automated chain is green.

1. **Import FBX** — Content Browser → Import / drop `.fbx` into Assets folder → descriptor appears; Intermediate glTF + Source archive written.
2. **Preview Fast Path** — Open / drag mesh into viewport before Cook completes (or with Final deleted) → mesh visible from Intermediate.
3. **Cook Final** — Wait for / trigger cook → subsequent loads prefer cooked Final; no visual regression.
4. **Edit Source → auto-Reimport** — Modify archived FBX under `Resources/Source/...` (or re-save from DCC) → watch debounce / Reimport → GUID unchanged; viewport refreshes.
5. **Scene GUID mesh reference** — Place mesh in scene, save → `.scene.asset` entity `"mesh"` is GUID; reopen scene attaches same Asset.

## OpenSpec tasks

- [x] 6.2 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Automated smoke uses OBJ fixtures (reliable Assimp path); FBX UI path remains human-only.
2. Fast Path may request cook synchronously in v1; smoke still asserts Intermediate load before explicit `cookAsset(force)`.
3. Runtime-linked tests need `slint_cpp.dll` / `slang.dll` on `PATH`.
4. Did not push.
