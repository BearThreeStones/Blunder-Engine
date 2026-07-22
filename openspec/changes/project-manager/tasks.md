## 1. Project File and Project List libraries

- [x] 1.1 Add Project File helpers: read/write `project.blunder` (YAML `name`), validate path is a Project
- [x] 1.2 Add Create scaffold: empty/new dir rules, optional Create Folder subdir, write marker + `Assets/` / `Resources/` / `.blunder/`
- [x] 1.3 Add Project List store (user config path): load/save, add/update by absolute path, remove, missing detection
- [x] 1.4 Unit tests: Project File parse/reject; Create non-empty reject; list add/remove/dedupe/missing

## 2. CLI entry and Editor Session root

- [x] 2.1 `engine_editor` resolves `--project-root` / Debug `BLUNDER_PROJECT_ROOT` (ADR 0010); no Manager mode
- [x] 2.2 Pass resolved project root into `FileSystem` / engine init for Editor Session
- [x] 2.3 Add spawn helper: Project Manager starts sibling `engine_editor` with `--project-root` and exits on success
- [x] 2.4 Note in `CONTENT_LAYOUT.md` / `CONTEXT.md`: user Projects under `E:/Blunder Projects`; engine checkout is not a Project (no root `project.blunder` / `Assets` / `Resources`)
- [x] 2.5 Project Manager Create prefills `defaultProjectsDirectory()` (`E:/Blunder Projects` on Windows)
- [x] 2.6 Migrate sample content to `E:/Blunder Projects/Test`; `BLUNDER_DEFAULT_PROJECT_ROOT` / Debug / cook-assets point there
- [x] 2.5 Build `project_manager` as a separate executable co-located with `engine_editor`

## 3. Project Manager UI

- [x] 3.1 Spike minimal Manager bootstrap (Slint + SDL without full Editor Session cook/scene load)
- [x] 3.2 Slint Project Manager: list (name/path/missing), Open, Remove, Create dialog, Import dialog
- [x] 3.3 Wire Create → scaffold + list + Open spawn; Import → validate + list + Open spawn
- [x] 3.4 Wire missing Open error; Remove from list only; persist list across Manager restarts

## 4. Validation

- [x] 4.1 Build with documented preset (`vs2026-debug` or equivalent); run new unit tests
- [x] 4.2 Manual smoke: `project_manager` entry, Create & Open, Import Dev Project, Remove, missing mark, Debug `engine_editor` without CLI
- [x] 4.3 Confirm `CONTEXT.md` and ADRs 0009/0010 match shipped behavior
