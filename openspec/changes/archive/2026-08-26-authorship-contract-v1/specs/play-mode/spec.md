## ADDED Requirements

### Requirement: Play Scripts build is not Diagnose
The Play Scripts build step SHALL remain a mutating Play gate. It SHALL NOT be Diagnose and SHALL NOT be an Op. A failed build SHALL keep the editor Stopped and SHALL report an Error-grade Issue with code `scripts.build_failed`. Diagnose MAY still report Scripts dirty or missing output without compiling.

#### Scenario: Failed build is Error Issue
- **WHEN** Scripts are dirty and `dotnet build` fails during Play start
- **THEN** the Player is not started
- **AND** the failure is an Error Issue with code `scripts.build_failed`

#### Scenario: Diagnose does not replace the build
- **WHEN** Scripts are dirty and the author starts Play
- **THEN** Play still invokes the Scripts build when dirty
- **AND** a prior Diagnose Warning for `scripts.dirty` does not skip that build

## MODIFIED Requirements

### Requirement: Scripts build when dirty before Play
Before spawning the Player, the editor SHALL build Project Scripts when sources are newer than the last successful scripts output; otherwise it SHALL reuse `.blunder/scripts_bin`. A failed build SHALL keep the editor Stopped and SHALL report an Error-grade Issue with code `scripts.build_failed` rather than a parallel stringly error type. This build SHALL NOT be Diagnose.

#### Scenario: Dirty Scripts block failed build
- **WHEN** Scripts are dirty and `dotnet build` fails
- **THEN** the Player is not started

#### Scenario: Clean Scripts skip build
- **WHEN** Scripts outputs are up to date
- **THEN** Play may proceed without invoking a new build

### Requirement: Play start camera gate
Play Mode start SHALL run Diagnose of the Play rule set for Camera on the Play entry scene after the scene path is known (as it will be loaded after dirty-prompt save rules) and before Player spawn. An Error-grade `play.missing_camera` Issue SHALL leave the editor in a non-Playing state for that attempt (no Player process). The camera gate SHALL NOT be a separate stringly error type beside Issue.

#### Scenario: Preflight runs before spawn
- **WHEN** the user starts Play and the entry scene has no Camera
- **THEN** the engine SHALL report Error Issue `play.missing_camera` and SHALL NOT spawn Player
