# dogwalk-tracy-profiler Specification

## Purpose

Human acceptance for the basic profiler is DogWalk `se-world.scene.asset` in windowed `engine_editor` and Play, not Test, Sponza, or collision QC.

## ADDED Requirements

### Requirement: DogWalk windowed walk is the acceptance scene
Human acceptance for this change SHALL use the DogWalk Project main scene `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and windowed Play. Test and Sponza SHALL NOT be the demo Project. Collision QC `root.scene.asset` SHALL NOT be this walk. 138 Spot lights, the dog walking, and a VRS rate-mask SHALL NOT be required. This change SHALL NOT modify or merge PR 24, PR 36, or PR 37. Cloud lavapipe GPU milliseconds SHALL NOT be this acceptance.

#### Scenario: Editor HUD and dock walk
- **WHEN** the human opens DogWalk `se-world.scene.asset` in windowed `engine_editor`
- **AND** the Frame timing HUD is on
- **THEN** the Viewport SHALL show CPU ms, GPU ms, and main Pass times
- **AND** the Profiler dock SHALL show a frame strip from the engine ring without Tracy.exe

#### Scenario: Play HUD walk
- **WHEN** the human runs windowed Play on that same scene
- **THEN** Play SHALL show the Frame timing HUD when toggled
- **AND** Play SHALL NOT show the Profiler dock

#### Scenario: Optional Tracy connect
- **WHEN** a development build with Tracy Client defined is running that scene
- **AND** the human connects Tracy.exe to localhost
- **THEN** Tracy SHALL show tick `FrameMark` and Pass GPU zones
- **AND** HUD and dock SHALL still show numbers if that connection is closed
