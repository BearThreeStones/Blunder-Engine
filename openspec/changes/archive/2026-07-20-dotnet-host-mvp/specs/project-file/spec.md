## MODIFIED Requirements

### Requirement: Project Create scaffold
Project Create SHALL write `project.blunder` with the chosen name, ensure `Assets/`, `Resources/`, `.blunder/`, and `Scripts/` exist under the Project root, write a minimal `net10.0` game `.csproj` under `Scripts/` that references Blunder.Api beside the editor/runtime, add the Project to the Project List, and then Project Open the new Project.

#### Scenario: Create writes marker and scaffold
- **WHEN** the user creates a Project named "Demo" in a valid empty target path
- **THEN** `project.blunder` exists with that name, `Assets/`, `Resources/`, `.blunder/`, and `Scripts/` exist, `Scripts/` contains a `.csproj`, the Project is listed, and an Editor Session is started via Project Open
