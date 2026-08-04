## ADDED Requirements

### Requirement: Method tracks on AnimationClip
AnimationClip Intermediate YAML SHALL support **method tracks** (timed logical event name + optional args). During playback the engine SHALL dispatch on **key-crossing** using the **base dominant-clip clock** (OneShot clock while OneShot is active) — the same clock family as Animation step. Delivery SHALL target co-located Behaviours and MAY use Message. The engine SHALL NOT PtrCall arbitrary C# methods by string as the product path. Audio tracks SHALL NOT be required for Phase 5.

#### Scenario: Key-crossing dispatch
- **WHEN** playback advances across a method key on the dominant clip clock
- **THEN** co-located Behaviours receive the event (name + args) without requiring script-only PlaybackPosition table scans

#### Scenario: OneShot uses OneShot clock
- **WHEN** OneShot is active and a method key on the OneShot clip is crossed
- **THEN** dispatch uses the OneShot clock, not a blended multi-clip method clock
