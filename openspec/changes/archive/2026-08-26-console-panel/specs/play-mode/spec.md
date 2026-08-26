## ADDED Requirements

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
