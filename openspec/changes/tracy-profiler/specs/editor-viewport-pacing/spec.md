## ADDED Requirements

### Requirement: Profiler frame boundary is the engine tick
Frame timing HUD, Frame timing ring, and Tracy `FrameMark` SHALL treat `tickOneFrame` as the frame boundary. They SHALL NOT treat Skia Present or SDL_RenderPresent as that boundary. Interactive and idle Present pacing SHALL remain as today’s viewport pacing; this profiler SHALL NOT disable idle pacing or force a Present per tick.

#### Scenario: Idle still throttles Present
- **WHEN** the editor viewport is idle
- **AND** the Frame timing HUD is on
- **THEN** Skia Present SHALL still follow idle pacing
- **AND** the Frame timing ring SHALL still receive one slot per engine tick

#### Scenario: FrameMark is not Present
- **WHEN** Tracy Client is compiled in
- **THEN** `FrameMark` SHALL be emitted from the engine tick
- **AND** Present SHALL NOT be required for that mark
