## ADDED Requirements

### Requirement: CLI play-frame uses Headless Play
CLI play-frame SHALL run on the Headless Editor Play session: spawn a Headless Player, use last saved Play entry when the Live document is dirty, take one Play frame, and Stop before the editor process exits. `--steps` default 0 SHALL take the frame while Playing after ready. `--steps` N greater than 0 SHALL Pause, Play step N at dt 1/60, then take the frame.

#### Scenario: CLI play-frame stops the Player
- **WHEN** CLI play-frame completes successfully
- **THEN** the Play Process is Stopped
- **AND** no Player is left running after the editor exits
