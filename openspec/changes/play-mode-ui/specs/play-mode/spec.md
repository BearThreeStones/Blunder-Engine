## ADDED Requirements

### Requirement: Editor exposes Play Pause and Stop
The editor SHALL provide Play, Pause, and Stop controls for the Play session. Play starts or resumes the session; Pause freezes gameplay Tick in the Player without ending the process; Stop ends the Play Process and returns the UI to Edit Mode (Stopped).

#### Scenario: Play from Stopped
- **WHEN** the author activates Play while Stopped and preflight succeeds
- **THEN** the editor enters a Starting/Playing state and a Player process is spawned

#### Scenario: Pause while Playing
- **WHEN** the author activates Pause while Playing
- **THEN** the editor shows Paused and the Player stops advancing Behaviour Tick until Resume/Play

#### Scenario: Stop ends session
- **WHEN** the author activates Stop while Playing or Paused
- **THEN** the Play Process is requested to exit and the editor returns to Stopped

### Requirement: At most one Play session
The editor SHALL allow at most one Play Process at a time. Starting Play while a session is active SHALL stop the existing session before starting a new one.

#### Scenario: Play replaces running session
- **WHEN** Play is requested while a Player is already running
- **THEN** the existing Player is stopped before a new Player is started

### Requirement: Dirty scene prompt before Play
When Play is requested and the active scene document is dirty, the editor SHALL prompt: save then Play, Play using the last saved asset, or cancel. It SHALL NOT silently auto-save or silently play without indication when dirty.

#### Scenario: Save and Play
- **WHEN** the author chooses Save and Play on a dirty scene
- **THEN** the scene is saved and Play proceeds using that saved asset

#### Scenario: Cancel dirty prompt
- **WHEN** the author chooses Cancel on the dirty prompt
- **THEN** Play does not start and the editor remains Stopped

### Requirement: Scripts build when dirty before Play
Before spawning the Player, the editor SHALL build Project Scripts when sources are newer than the last successful scripts output; otherwise it SHALL reuse `.blunder/scripts_bin`. A failed build SHALL keep the editor Stopped and surface an error.

#### Scenario: Dirty Scripts block failed build
- **WHEN** Scripts are dirty and `dotnet build` fails
- **THEN** the Player is not started

#### Scenario: Clean Scripts skip build
- **WHEN** Scripts outputs are up to date
- **THEN** Play may proceed without invoking a new build

### Requirement: Edit Mode remains editable during Play
While a Play session is running, the author SHALL be able to continue editing the Project in the editor. Those edits SHALL NOT appear in the live Player until a later Play after save/build rules.

#### Scenario: Edit while Playing
- **WHEN** a Player is Playing and the author edits the active scene
- **THEN** the editor accepts the edits and the running Player world is unchanged
