# Task 5 Report — Play preflight gates

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 5.1–5.3)

| Item | Result |
|------|--------|
| 5.1 Dirty scene prompt | Done — pure `decidePlayDirtyScene` + Slint `PlayDirtySceneDialog` (Save and Play / Play Last Saved / Cancel) |
| 5.2 Scripts dirty + build | Done — `areProjectScriptsDirty` + `runPlayScriptsGate`; `PlaySessionController::play` runs gate before spawn; fail stays Stopped |
| 5.3 Tests | Done — `play_preflight_test` covers decisions, FS dirty heuristic, fake build gate, controller abort |

## Commits

1. `5fbbe32` — `feat(editor): Play dirty-scene and Scripts build preflight`

## TDD evidence

### RED

CMake configure failed: missing `../runtime/project/play_preflight.cpp` for `play_preflight_test`.

Logged: `.superpowers/sdd/task-5-red-cmake.txt`, `task-5-red-build.txt`

### GREEN

```text
play_preflight_test: all passed
EXIT=0
play_session_controller_test: all passed
```

Logged: `.superpowers/sdd/task-5-green-build.txt`, `task-5-green-run.txt`  
Also: `engine_runtime` rebuild (Slint dirty dialog + UiHost wiring) OK — `task-5-runtime-build.txt`.

## Design notes

- Dirty prompt decision is pure; UiHost shows dialog when dirty without a choice, then applies Save/LastSaved/Cancel.
- Scripts gate is injectable on `PlaySessionController` via `setScriptsPreflight`; UiHost installs real `areProjectScriptsDirty` + `buildProjectScripts` before spawn.
- Dirty dialog mirrors ImportMeshDialog modal pattern (blocks camera input while open).

## Concerns

1. **Scripts dirty heuristic** — mtime of `.cs`/`.csproj` vs game DLLs under `scripts_bin`; does not inspect NuGet restore / intermediate objs.
2. **Build errors not toasted** — failed `dotnet build` sets `lastError()` but no Slint toast yet.
3. **Save and Play** relies on `EditorSceneEditSystem::saveActiveScene()` success; save failure still attempts Play after decision.
4. **No e2e dirty-dialog smoke** in this task (unit + runtime build only).
