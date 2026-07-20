## ADDED Requirements

### Requirement: Scripts root layout
A Project SHALL use a root folder `Scripts/` for C# gameplay sources and the game `.csproj`. Scripts SHALL NOT be treated as Content Browser Assets under `Assets/` / `Resources/`.

#### Scenario: Scripts is not content pipeline input
- **WHEN** a Project contains `Scripts/*.cs`
- **THEN** those files are built and loaded by the .NET script host path, not cooked as Intermediate Assets

### Requirement: Create scaffolds Scripts
Project Create SHALL create `Scripts/` with a minimal `net10.0` game `.csproj` that references Blunder.Api via a path relative to the editor/runtime install directory (HintPath or equivalent).

#### Scenario: New Project has Scripts csproj
- **WHEN** the user creates a new Project
- **THEN** `Scripts/` exists under the Project root and contains a `.csproj` targeting `net10.0`

### Requirement: Dev build via dotnet
The engine SHALL provide a Scripts build helper that invokes `dotnet build` on the Project Scripts project and places output under `.blunder/scripts_bin` (or an equivalent Project-local cache path).

#### Scenario: Build produces game DLL
- **WHEN** ScriptsBuilder runs successfully on a Project with a valid Scripts csproj and .NET 10 SDK
- **THEN** a game assembly DLL exists in the Scripts build output path for the host to load
