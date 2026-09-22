## ADDED Requirements

### Requirement: Windowed Player presents through a HUD-only Slint root
A windowed Player SHALL present the Play-rule camera color through a HUD-only Slint root that shares the engine Vulkan device (3D stays offscreen; Slint composites color + Frame timing HUD). It SHALL NOT use SDL_Renderer CPU blit as the windowed present path. It SHALL NOT mount the editor shell (`editor_window.slint` docks, Application Bar, authorship panels). It SHALL NOT mount the 2027-01 game shell (start / settings / pause). Editor Overlays SHALL stay off. Headless Player SHALL keep no OS window and SHALL NOT mount that Slint HUD root.

#### Scenario: Windowed Play composites HUD on shared device
- **WHEN** a windowed Player presents a frame
- **THEN** present SHALL be Slint composition of the offscreen 3D color plus optional Frame timing HUD
- **AND** Editor Overlays SHALL not draw
- **AND** editor docks SHALL not appear

#### Scenario: Headless Play has no HUD root
- **WHEN** a Headless Player is running
- **THEN** no Player OS window exists
- **AND** no Frame timing HUD SHALL be shown
- **AND** a Play frame SHALL still be sent on the Play control channel

### Requirement: Windowed Player Frame timing HUD
A windowed Player SHALL offer the same Frame timing HUD readout as the editor Viewport (FPS, CPU ms, GPU ms, main Pass table, packed `inst` / `batches` / surviving `cmds`, lights). F3 SHALL toggle it. Default SHALL be off and SHALL NOT persist. Title-bar FPS MAY remain. Title-bar FPS SHALL NOT be the only frame-time readout. The Player SHALL NOT show the Profiler dock.

#### Scenario: Player HUD is togglable
- **WHEN** the author presses F3 in a windowed Player
- **THEN** the Frame timing HUD SHALL toggle
- **AND** the Profiler dock SHALL not appear

#### Scenario: Title FPS is not the only readout
- **WHEN** a windowed Player is running with the HUD on
- **THEN** CPU ms, GPU ms, and main Pass times SHALL be visible in the HUD
- **AND** a title-bar FPS string SHALL not be the sole timing UI
