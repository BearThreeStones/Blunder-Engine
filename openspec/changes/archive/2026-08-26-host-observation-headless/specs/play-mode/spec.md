## ADDED Requirements

### Requirement: Headless Editor Play session
A Headless Editor SHALL run a Play session (spawn Player, pause, resume, stop, Play step, Play frame) on the Play control channel without UiHost or Slint Play controls. A Headless Editor SHALL spawn a Headless Player.

#### Scenario: Headless Play without editor chrome
- **WHEN** a Headless Editor starts Play and preflight succeeds
- **THEN** a Headless Player process is spawned
- **AND** the editor session becomes Starting/Playing without an editor OS window

## MODIFIED Requirements

### Requirement: Dirty scene prompt before Play
When Play is requested on a windowed Editor and the active scene document is dirty, the editor SHALL prompt: save then Play, Play using the last saved asset, or cancel. It SHALL NOT silently auto-save or silently play without indication when dirty. A Headless Editor SHALL NOT show that prompt; Play SHALL use the last saved Play entry asset (same as the windowed "play last saved" choice). Live Capture remains the Live document.

#### Scenario: Save and Play
- **WHEN** the author chooses Save and Play on a dirty scene
- **THEN** the scene is saved and Play proceeds using that saved asset

#### Scenario: Cancel dirty prompt
- **WHEN** the author chooses Cancel on the dirty prompt
- **THEN** Play does not start and the editor remains Stopped

#### Scenario: Headless Play uses last saved
- **WHEN** a Headless Editor starts Play while the Live document is dirty
- **THEN** no dirty-scene prompt is shown
- **AND** the Player loads the last saved Play entry asset
