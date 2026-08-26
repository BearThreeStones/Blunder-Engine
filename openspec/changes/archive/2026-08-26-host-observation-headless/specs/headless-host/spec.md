## Purpose

Boot Editor or Player with no OS window while keeping EngineHostMode Editor or Player — not a third host composition and not a different observation contract.

## ADDED Requirements

### Requirement: Headless is not a third host mode
Headless SHALL be an Editor Session or Player with no OS window. It SHALL remain `EngineHostMode` Editor or Player. The product SHALL NOT add `EngineHostMode::Headless` or a third process kind named `engine_agent`.

#### Scenario: Headless Editor is still Editor
- **WHEN** the Editor starts with no OS window
- **THEN** host composition is Editor
- **AND** Authorship System is mounted

#### Scenario: Headless Player is still Player
- **WHEN** the Player starts with no OS window
- **THEN** host composition is Player
- **AND** Authorship System is not mounted

### Requirement: Headless Editor omits the editor shell
A Headless Editor SHALL NOT mount Slint, UiHost, or the viewport sink/bridge. It SHALL still mount Authorship System and a Play session capable of spawning a Player.

#### Scenario: No Slint in Headless Editor
- **WHEN** a Headless Editor is running
- **THEN** there is no editor OS window and no Slint editor shell

#### Scenario: Authorship still mounted
- **WHEN** a Headless Editor is running
- **THEN** Query / Op / Diagnose remain available on the Authorship contract

### Requirement: Headless Player has no OS window
A Headless Player SHALL NOT create an OS window. Session control SHALL use the Play control channel. Ending the process SHALL be Stop on that channel or process exit. Window close SHALL NOT be the Headless end path.

#### Scenario: No Player window
- **WHEN** a Headless Player is running
- **THEN** no Player OS window exists

#### Scenario: Stop ends Headless Player
- **WHEN** the editor session sends Stop while a Headless Player is running
- **THEN** the Player process exits
