## ADDED Requirements

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
