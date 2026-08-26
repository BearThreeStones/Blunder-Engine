# play-mode Specification

## Purpose
Editor-side Play session: Play/Pause/Stop controls, dirty-scene and Scripts-dirty preflight, single Player spawn, Edit Mode remains editable during Play.

## Requirements

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
Before spawning the Player, the editor SHALL build Project Scripts when sources are newer than the last successful scripts output; otherwise it SHALL reuse `.blunder/scripts_bin`. A failed build SHALL keep the editor Stopped and SHALL report an Error-grade Issue with code `scripts.build_failed` rather than a parallel stringly error type. This build SHALL NOT be Diagnose.

#### Scenario: Dirty Scripts block failed build
- **WHEN** Scripts are dirty and `dotnet build` fails
- **THEN** the Player is not started

#### Scenario: Clean Scripts skip build
- **WHEN** Scripts outputs are up to date
- **THEN** Play may proceed without invoking a new build

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

### Requirement: Edit Mode remains editable during Play
While a Play session is running, the author SHALL be able to continue editing the Project in the editor. Those edits SHALL NOT appear in the live Player until a later Play after save/build rules.

#### Scenario: Edit while Playing
- **WHEN** a Player is Playing and the author edits the active scene
- **THEN** the editor accepts the edits and the running Player world is unchanged

### Requirement: Play start camera gate
Play Mode start SHALL run Diagnose of the Play rule set for Camera on the Play entry scene after the scene path is known (as it will be loaded after dirty-prompt save rules) and before Player spawn. An Error-grade `play.missing_camera` Issue SHALL leave the editor in a non-Playing state for that attempt (no Player process). The camera gate SHALL NOT be a separate stringly error type beside Issue.

#### Scenario: Preflight runs before spawn
- **WHEN** the user starts Play and the entry scene has no Camera
- **THEN** the engine SHALL report Error Issue `play.missing_camera` and SHALL NOT spawn Player

### Requirement: Play Pause does not unlock Editor Camera
While Play is paused, the Player view SHALL remain the scene Camera resolve path and SHALL NOT enable Editor Camera orbit or authorship input.

#### Scenario: Pause keeps authorship off
- **WHEN** Play is paused and the user attempts Editor Camera orbit in the Player window
- **THEN** the view SHALL NOT orbit via Editor Camera

### Requirement: Editor ingests Play log forwarding
While a Play session is connected, the editor SHALL read Play Process Console Messages from the Play control channel and append them to the Console ring with Play Process origin. Stop SHALL NOT drop those rows.

#### Scenario: Forwarded Log appears in editor
- **WHEN** Play is Playing and the Player emits a Log Console Message on the control channel
- **THEN** the editor Console lists that message with Play Process origin

### Requirement: Clear on Play at session start
When Clear on Play is enabled, the editor SHALL perform Console clear when a Play session starts after successful preflight (as the Player is spawned). When the toggle is off, existing rows SHALL remain.

#### Scenario: Toggle off keeps prior rows
- **WHEN** Clear on Play is off, the Console has rows, and Play starts
- **THEN** those rows remain visible alongside new Player messages

### Requirement: Error Pause uses Play Pause
When Error Pause is enabled and a Play Process origin Error is ingested while Playing, the editor SHALL send pause on the Play control channel (same command as the Pause control).

#### Scenario: Error Pause sends pause command
- **WHEN** Error Pause is on and a forwarded Error arrives while Playing
- **THEN** the editor session becomes Paused using the existing Pause path

### Requirement: Play step while paused
Play step SHALL be legal only while Play Pause. It SHALL advance the Play Process by N gameplay Ticks at fixed dt 1/60 and SHALL remain paused. Unpaused Play SHALL stay realtime. Play step SHALL NOT be an Op and SHALL NOT use wall-clock sleep. `BLUNDER_PLAYER_MAX_FRAMES` SHALL NOT be Play step.

#### Scenario: Step from Pause
- **WHEN** the Play session is Paused and Play step requests 30 ticks
- **THEN** the Player advances 30 gameplay Ticks at dt 1/60
- **AND** the session remains Paused

#### Scenario: Step while Playing fails
- **WHEN** the Play session is Playing (not Paused) and Play step is requested
- **THEN** the Player does not apply those ticks as Play step
- **AND** realtime Play is unchanged

### Requirement: Play frame on the control channel
The Play control channel SHALL carry Play step and Play frame in addition to pause, resume, and stop. Player → editor Play frames SHALL use that same connection. The channel SHALL NOT add a second Play-session socket for frames.

#### Scenario: Frame after step
- **WHEN** the session is Paused and Play step then Play frame run
- **THEN** the editor receives a 16:9 Play frame of the Play Process world

#### Scenario: One socket
- **WHEN** Play frame is requested
- **THEN** it uses the existing Play control channel
