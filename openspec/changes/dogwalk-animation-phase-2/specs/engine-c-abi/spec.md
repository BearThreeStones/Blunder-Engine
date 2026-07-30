## ADDED Requirements

### Requirement: AnimationPlayer two-slot and TimeScale on C-ABI
The C-ABI SHALL expose entry points to assign AnimationPlayer slot clips by logical name, set blendWeight in [0, 1], get/set global TimeScale, and Play with an optional fade duration, in addition to existing Play/Stop/Loop/PoseApplied surfaces. ABI version SHALL bump when these entries ship, and NativeAbi fill/completeness SHALL include them.

#### Scenario: Set blend weight via C-ABI
- **WHEN** a caller sets blendWeight on a valid Object with AnimationPlayer through the C-ABI
- **THEN** subsequent sampling uses that weight on the two-slot model

#### Scenario: Play with fade via C-ABI
- **WHEN** a caller invokes Play with fade duration greater than zero through the C-ABI
- **THEN** the native AnimationPlayer Crossfades on the two-slot model

#### Scenario: TimeScale via C-ABI
- **WHEN** a caller sets TimeScale through the C-ABI
- **THEN** AnimationPlayer advance respects that global multiplier
