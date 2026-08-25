## MODIFIED Requirements

### Requirement: Add clip is not an Add… item
The AnimationPlayer Inspector section SHALL provide **Add clip**, which opens an AnimationClip picker and, on confirm, appends one complete **Clip Binding** (logical name defaults to the clip stem; duplicate names rejected). Cancel SHALL add no row. Clips SHALL NOT appear in Add…. Empty name→GUID draft rows SHALL NOT be created by Add clip. Content Browser drop onto the clip list or an existing row SHALL follow Clip Binding authorship rules (append vs retarget). Import SHALL NOT auto-fill the clip map.

#### Scenario: Empty map after Add Player
- **WHEN** the author adds AnimationPlayer and then Add clip, confirms an AnimationClip whose stem is `LOOP-chocomel-idle`
- **THEN** the clip map has one complete binding named `LOOP-chocomel-idle` referencing that clip
- **AND** the Inspector does not show an empty name/GUID draft row

#### Scenario: Add clip cancel
- **WHEN** the author adds AnimationPlayer, clicks Add clip, and cancels the picker
- **THEN** the clip map remains empty
