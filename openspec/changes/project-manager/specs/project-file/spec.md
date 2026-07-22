## ADDED Requirements

### Requirement: Project File identity
A Project SHALL be identified by a Project File named `project.blunder` at the Project root. The file SHALL be YAML and SHALL include a `name` field used as the display name. A folder without this file SHALL NOT be treated as a Blunder Project for Import or Open validation.

#### Scenario: Valid project has project.blunder
- **WHEN** a directory contains `project.blunder` with a `name` field
- **THEN** Import and Open validation treat that directory as a Project

#### Scenario: Folder without Project File is rejected
- **WHEN** the user attempts to Import a folder that has `Assets/` but no `project.blunder`
- **THEN** Import fails validation and the folder is not added as a Project

### Requirement: Project Create scaffold
Project Create SHALL write `project.blunder` with the chosen name, ensure `Assets/`, `Resources/`, and `.blunder/` exist under the Project root, add the Project to the Project List, and then Project Open the new Project.

#### Scenario: Create writes marker and scaffold
- **WHEN** the user creates a Project named "Demo" in a valid empty target path
- **THEN** `project.blunder` exists with that name, the three scaffold directories exist, the Project is listed, and an Editor Session is started via Project Open

### Requirement: Create path must be empty or new
Project Create SHALL require the target Project directory to be empty or not yet exist. When Create Folder is enabled, the implementation SHALL create a subdirectory derived from the project name under the chosen parent and use that subdirectory as the Project root. Create SHALL NOT overwrite an existing `project.blunder`.

#### Scenario: Non-empty target rejected
- **WHEN** the user selects a non-empty directory as the Create target without Create Folder producing a new empty child
- **THEN** Create is rejected and no Project File is written

#### Scenario: Create Folder uses named subdirectory
- **WHEN** Create Folder is enabled and the parent path is valid
- **THEN** the Project is created in a new subdirectory based on the project name

### Requirement: Engine checkout is not a Project
The Blunder Engine repository root SHALL NOT include a committed `project.blunder`, `Assets/`, or `Resources/`. User Projects live under the canonical Projects directory (`E:/Blunder Projects` on Windows). Debug `BLUNDER_PROJECT_ROOT` SHALL default to `E:/Blunder Projects/Test` (`BLUNDER_DEFAULT_PROJECT_ROOT`).

#### Scenario: Checkout has no Project File
- **WHEN** Import or Project List validation runs against the engine repository root
- **THEN** it is not treated as a Blunder Project because `project.blunder` is absent

#### Scenario: Create defaults to Projects directory
- **WHEN** the user opens the Create dialog
- **THEN** the path field is prefilled with the canonical Projects directory

#### Scenario: Debug opens Test Project
- **WHEN** a Debug `engine_editor` is launched without `--project-root`
- **THEN** it opens `E:/Blunder Projects/Test` via `BLUNDER_PROJECT_ROOT`
