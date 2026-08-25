## MODIFIED Requirements

### Requirement: Add… and Remove attachment are Editor Commands
Successfully applying Add… (including host cascade and Skeleton hydration), Remove attachment, Add clip (confirmed picker result), clip retarget (drop-on-row or per-row picker), or clip-row Remove SHALL push a single Editor Command on Document History for the active scene. Commands SHALL target EntityId. One author click SHALL be one Command: cascaded hosts and hydration SHALL undo and redo together, not as separate history steps. Canceling the Add clip picker SHALL NOT push a Command.

#### Scenario: Undo Add AnimationPlayer cascade
- **WHEN** the author adds AnimationPlayer (creating Skeleton and hydrating bones) and then undoes
- **THEN** AnimationPlayer, the created Skeleton, and hydrated bones are gone, and Document History does not leave a leftover Skeleton-only step

#### Scenario: Undo Add Camera
- **WHEN** the author adds Camera from Add… and then undoes
- **THEN** that entity has no Camera Component

#### Scenario: Redo Add clip
- **WHEN** the author confirms Add clip for a complete Clip Binding, undoes, then redoes
- **THEN** that complete binding is restored on that entity’s AnimationPlayer

#### Scenario: Cancel Add clip pushes no history
- **WHEN** the author opens Add clip and cancels the picker
- **THEN** Document History has no new clip-bindings Command
