# project-manager Specification

## Purpose

Standalone Project Manager executable entry, Project Open via spawning `engine_editor`, and list actions (Remove, missing marking) for the Project Manager MVP.

## Requirements

### Requirement: Project Manager is a separate executable
The Project Manager SHALL be built as its own executable (`project_manager`) that lists known Projects and provides Create, Import, Open, and Remove-from-list actions. It SHALL NOT be a launch mode of `engine_editor`.

#### Scenario: Launching project_manager shows Manager
- **WHEN** the user launches `project_manager`
- **THEN** the Project Manager UI is shown instead of a full Editor Session

#### Scenario: Explicit project root opens Editor Session
- **WHEN** the user launches `engine_editor` with `--project-root` pointing at a valid Project
- **THEN** an Editor Session starts for that Project root and Project Manager is not shown

### Requirement: Debug engine_editor may omit --project-root
Debug builds of `engine_editor` that define `BLUNDER_PROJECT_ROOT` MAY open that root as an Editor Session when no `--project-root` is given. Release builds of `engine_editor` SHALL require `--project-root` (or fail with a message pointing at `project_manager`).

#### Scenario: Release without project root fails
- **WHEN** a Release `engine_editor` is launched without `--project-root`
- **THEN** startup fails with an error that mentions `project_manager`

### Requirement: Project Open spawns the editor
Choosing Open (including after successful Create or Import) SHALL start an Editor Session by spawning sibling `engine_editor` with `--project-root` set to the Project root. v1 SHALL NOT hot-swap the project root inside an already-running Editor Session.

#### Scenario: Open spawns engine_editor with project root
- **WHEN** the user opens a Project from Project Manager
- **THEN** a new `engine_editor` process is started with that Project's root and the Manager process exits (or yields) after a successful spawn

### Requirement: Remove from list does not delete files
Remove SHALL delete the entry from the Project List only. It SHALL NOT delete the Project directory or Project File on disk by default.

#### Scenario: Remove keeps disk project
- **WHEN** the user removes a Project from the Project List
- **THEN** the list no longer shows that entry and the on-disk Project files remain

### Requirement: Missing path marking
When loading the Project List, entries whose path or Project File is missing SHALL be marked missing in the UI and SHALL remain removable. Opening a missing entry SHALL fail with an error and SHALL NOT spawn an Editor Session.

#### Scenario: Missing project shown but not opened
- **WHEN** a listed Project path no longer contains a Project File
- **THEN** the entry is marked missing and Open does not start an Editor Session
