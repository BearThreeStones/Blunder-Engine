# project-file Specification

## Purpose

On-disk Project identity (`project.blunder`) and Create scaffold rules for Blunder Projects.

## Requirements

### Requirement: Project File identity
A Project SHALL be identified by a Project File named `project.blunder` at the Project root. The file SHALL be YAML and SHALL include a `name` field used as the display name. A folder without this file SHALL NOT be treated as a Blunder Project for Import or Open validation.

#### Scenario: Valid project has project.blunder
- **WHEN** a directory contains `project.blunder` with a `name` field
- **THEN** Import and Open validation treat that directory as a Project

#### Scenario: Folder without Project File is rejected
- **WHEN** the user attempts to Import a folder that has `Assets/` but no `project.blunder`
- **THEN** Import fails validation and the folder is not added as a Project

### Requirement: Project Create scaffold
Project Create SHALL write `project.blunder` with the chosen name, ensure `Assets/`, `Resources/`, `.blunder/`, and `Scripts/` exist under the Project root, write a minimal `net10.0` game `.csproj` under `Scripts/` that references Blunder.Api beside the editor/runtime, add the Project to the Project List, and then Project Open the new Project.

#### Scenario: Create writes marker and scaffold
- **WHEN** the user creates a Project named "Demo" in a valid empty target path
- **THEN** `project.blunder` exists with that name, `Assets/`, `Resources/`, `.blunder/`, and `Scripts/` exist, `Scripts/` contains a `.csproj`, the Project is listed, and an Editor Session is started via Project Open

### Requirement: Create path must be empty or new
Project Create SHALL require the target Project directory to be empty or not yet exist. When Create Folder is enabled, the implementation SHALL create a subdirectory derived from the project name under the chosen parent and use that subdirectory as the Project root. Create SHALL NOT overwrite an existing `project.blunder`.

#### Scenario: Non-empty target rejected
- **WHEN** the user selects a non-empty directory as the Create target without Create Folder producing a new empty child
- **THEN** Create is rejected and no Project File is written

#### Scenario: Create Folder uses named subdirectory
- **WHEN** Create Folder is enabled and the parent path is valid
- **THEN** the Project is created in a new subdirectory based on the project name

### Requirement: Dev Project marker in engine checkout
The Blunder Engine repository root SHALL include a committed `project.blunder` so the checkout is a valid Project for Debug entry, Import, and Project List.

#### Scenario: Repo root is importable
- **WHEN** a user Imports the engine repository root
- **THEN** validation succeeds because `project.blunder` is present
