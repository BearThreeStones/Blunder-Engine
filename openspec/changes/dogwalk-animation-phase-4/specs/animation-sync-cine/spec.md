## ADDED Requirements

### Requirement: Fire through active AnimationTree as OneShot
When Sync Group Fire targets a member whose Object has an **active AnimationTree**, the engine SHALL apply the member's clip instruction as an AnimationTree **OneShot** (insert then return to base). Fire SHALL NOT require deactivating the AnimationTree as the default path. Members without an active AnimationTree SHALL keep Phase 3 hard-cut `Play` semantics.

#### Scenario: Character tree + prop player sync
- **WHEN** Fire includes an active-tree character with clip `SYNC-attach` and a no-tree prop with clip `SYNC-prop`
- **THEN** the character receives a OneShot for `SYNC-attach` while the prop hard-cut Plays `SYNC-prop` at the same logical moment

#### Scenario: Fire does not deactivate tree
- **WHEN** Fire applies a OneShot to an active-tree member
- **THEN** AnimationTree remains active after the Fire (OneShot runs inside the tree)
