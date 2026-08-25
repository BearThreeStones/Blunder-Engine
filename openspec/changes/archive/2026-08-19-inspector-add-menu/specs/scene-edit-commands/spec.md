## ADDED Requirements

### Requirement: Add… and Remove attachment are Editor Commands
Successfully applying Add… (including host cascade and Skeleton hydration), Remove attachment, Add clip, or clip-row Remove SHALL push a single Editor Command on Document History for the active scene. Commands SHALL target EntityId. One author click SHALL be one Command: cascaded hosts and hydration SHALL undo and redo together, not as separate history steps.

#### Scenario: Undo Add AnimationPlayer cascade
- **WHEN** the author adds AnimationPlayer (creating Skeleton and hydrating bones) and then undoes
- **THEN** AnimationPlayer, the created Skeleton, and hydrated bones are gone, and Document History does not leave a leftover Skeleton-only step

#### Scenario: Undo Add Camera
- **WHEN** the author adds Camera from Add… and then undoes
- **THEN** that entity has no Camera Component

#### Scenario: Redo Add clip
- **WHEN** the author Add clips a row, undoes, then redoes
- **THEN** the empty name→GUID row is restored on that entity’s AnimationPlayer
