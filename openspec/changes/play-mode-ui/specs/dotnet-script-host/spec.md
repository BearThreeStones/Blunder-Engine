## ADDED Requirements

### Requirement: Product Play hosts DotNetHost in the Player
For product Play Mode, the .NET script host SHALL run inside the Player process. Edit Mode SHALL NOT require starting DotNetHost in the editor process for normal authorship. An environment-gated editor host MAY remain available for debug and tests only.

#### Scenario: Play without editor host
- **WHEN** the author enters Play Mode without `BLUNDER_DOTNET_SCRIPTS` set
- **THEN** the Player still starts its own DotNetHost for the session when the runtime is available

#### Scenario: Editor idle without host
- **WHEN** the editor runs in Edit Mode without the debug host env gate
- **THEN** the editor does not start DotNetHost solely to enable Play Mode
