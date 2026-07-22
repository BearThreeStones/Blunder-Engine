## Context

Today `engine_editor` ignored argv and `FileSystem` fell back to `BLUNDER_PROJECT_ROOT` (or the executable directory). There is no Project identity file, no user Project List, and no pre-editor UI. Content layout already assumes a project root with `Assets/`, `Resources/`, and `.blunder/`. ADRs 0009/0010 fix product shape: separate Project Manager executable, spawn editor on open, Debug editor shortcut optional.

## Goals / Non-Goals

**Goals:**
- Ship Project Manager MVP: list, create, import, open (spawn editor), remove from list, missing-path marking
- Define `project.blunder` + Create scaffold; commit Dev Project file at repo root
- Separate `project_manager` exe; `engine_editor` takes `--project-root` (Debug `BLUNDER_PROJECT_ROOT` shortcut per ADR 0010)
- Keep Editor Session initialization on a known root (no in-process root swap)

**Non-Goals:**
- Scan, Favorites, Rename, Duplicate, Tags, templates, Asset Library, Recovery Mode, ZIP install
- Multi-editor-version Hub management (Unity Hub–style)
- Hot-swap Project root inside a live Editor Session / return-to-Manager without spawn
- Engine-version gates in Project File

## Decisions

1. **Separate Project Manager executable** — `project_manager` hosts the Manager UI (`ProjectManagerApp` + Slint `project_manager_mode`). `engine_editor` only starts Editor Sessions. Both targets write to the same output directory so Open can resolve sibling `engine_editor`.

2. **Project Open spawns editor** — On Create/Import/Open success, persist list then **spawn** `engine_editor` with `--project-root <path>` and exit the Manager (ADR 0009). Prefer platform spawn over in-process re-init. Failure to spawn surfaces an error in Manager and leaves the list updated.

3. **Project File** — Path: `<project_root>/project.blunder`. Format: YAML with required `name` (string). Presence of the file defines a Project. Create also ensures `Assets/`, `Resources/`, `.blunder/` exist (empty dirs ok). Import accepts a directory containing the file or the file path itself.

4. **Project List store** — User-level file (e.g. `%APPDATA%/Blunder/project_list.yaml` on Windows; XDG config on Linux when needed). Entries: absolute path, display name cache, last-opened timestamp (optional), missing flag derived at load by checking path + Project File. Remove deletes the list entry only.

5. **Create rules** — Target must be empty or non-existent; optional Create Folder appends a sanitized project-name subdir under the chosen parent. Success → write Project File + scaffold → add to list → Open (spawn editor).

6. **Import rules** — Validate Project File → add/update list entry → Open (Import & Open).

7. **UI** — New Slint root for Project Manager (not the full editor shell). Native folder/file dialog for browse. List shows name, path, missing state; actions: Open, Remove, Create, Import.

8. **Dev Project** — Add committed `project.blunder` at repo root (`name: Blunder Engine` or similar) so Import/list and Debug root agree.

9. **Tests** — Unit-test Project File IO, Create validation (non-empty reject), Project List add/remove/missing, and editor session launch resolution without needing full Vulkan when possible.

## Risks / Trade-offs

- **[Risk] Full engine init before Manager is heavy → Mitigation:** Manager-only bootstrap (`ProjectManagerApp`); no cook/scene/thumbnails.
- **[Risk] Spawn fails (antivirus, path quoting, missing sibling exe) → Mitigation:** Clear error; keep Project List consistent; co-locate binaries; document quoting for spaces.
- **[Risk] Debug shortcut vs product entry confusion → Mitigation:** ADR 0010; document `project_manager` as product entry; Release never uses compile-time root as silent default.
- **[Risk] Repo-root Project File surprises packaging → Mitigation:** File is tiny metadata only; content roots unchanged.

## Migration Plan

1. Library: Project File + Project List + editor launch resolve (tests)
2. Commit `project.blunder`; teach Editor Session to honor `--project-root`
3. Project Manager Slint + spawn Open path; separate `project_manager` target
4. Document Debug shortcut for `engine_editor`; product entry is `project_manager`
5. Rollback: leave Project File inert if Manager disabled

## Open Questions

- ~~Exact Minimal bootstrap for Manager~~ — Resolved: `ProjectManagerApp` + `SlintSystem` `project_manager_mode` (WindowSystem + self-owned Skia Vulkan; no cook/scene).
- ~~Same binary vs separate exe~~ — Resolved: separate `project_manager` executable (user request).
- Project List filename/schema uses `version: 1` in YAML.
