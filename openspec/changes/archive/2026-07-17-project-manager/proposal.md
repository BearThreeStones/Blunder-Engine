## Why

The editor always opens a compile-time or implicit project root (`BLUNDER_PROJECT_ROOT`), so there is no Unity Hub / Godot–style way to create, import, or switch Projects. Authors need a Project Manager entry before a full Editor Session, with a clear on-disk Project identity (`project.blunder`).

## What Changes

- Add **`project_manager`** as a separate executable beside `engine_editor` for list/create/import/open/remove
- Introduce **Project File** (`project.blunder`, YAML with at least `name`) plus Create scaffold (`Assets/`, `Resources/`, `.blunder/`)
- Persist a user-level **Project List**; support Create, Import, Open, Remove from list, and missing-path marking
- **Project Open** spawns sibling `engine_editor` with `--project-root` (Editor Session); v1 does not hot-swap roots in-process
- Wire `engine_editor` CLI: `--project-root`; Debug may omit it via `BLUNDER_PROJECT_ROOT` (ADR 0010)
- Commit a root **`project.blunder`** so the engine checkout is a valid Dev Project
- **Out of scope (MVP):** Scan, Rename, Duplicate, Favorites, Tags, templates/Asset Library, Recovery Mode, multi-editor-version Hub, ZIP install, in-process return-to-Manager

## Capabilities

### New Capabilities
- `project-manager`: Project Manager UI and entry routing (list, create, import, open, remove, missing)
- `project-file`: On-disk Project identity (`project.blunder`) and Create scaffold rules
- `project-list`: User-level registry persistence and list metadata

### Modified Capabilities
- _(none — no existing OpenSpec capability covers editor project entry)_

## Impact

- Entry: new `engine/src/project_manager/`; `engine_editor` is Editor Session only
- Runtime: `FileSystem` already accepts `project_root`; Editor Session must use CLI root when present
- New: Project Manager Slint UI; Project List store under user config; Project File read/write helpers
- Repo: committed `project.blunder` at checkout root
- Docs: `CONTEXT.md` (done); ADRs 0009 / 0010 (done); `CONTENT_LAYOUT.md` mention of Project File
- Tests: Project File parse/create; Project List add/remove/missing; editor session launch resolution
