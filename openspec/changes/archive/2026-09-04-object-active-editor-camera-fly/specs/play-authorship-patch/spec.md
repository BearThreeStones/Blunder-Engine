## MODIFIED Requirements

### Requirement: v1 patch catalog is authored data on existing entities
v1 Play authorship patches SHALL cover authored data on entities that already exist in both the Live document and the Play Process world: Local Transform, Unique attachments, MeshRenderer, Object Active, Behaviour bags, SkeletonModifiers, and animation-host fields. Spawn, delete, rename, reparent, and Global Commands SHALL NOT be v1 patches. Those Commands SHALL remain editor-only until Play Reload or a later Play. Uncommitted gizmo or field samples SHALL NOT become patches.

#### Scenario: Transform seal patches
- **WHEN** the author seals a Local Transform Command on a Play-entry entity that exists in the Player
- **THEN** the Player writes that Local Transform onto that entity

#### Scenario: Object Active seal patches
- **WHEN** the author seals an Object Active Command on a Play-entry entity that exists in the Player
- **THEN** the Player writes that Object Active onto that entity

#### Scenario: Spawn stays editor-only
- **WHEN** the author seals a Spawn Command on the Play entry Live document while Playing
- **THEN** the editor does not send a v1 Play authorship patch for that Spawn
- **AND** the Play Process world does not gain that entity until Play Reload or a later Play

#### Scenario: Uncommitted gizmo sample is not a patch
- **WHEN** the author drags a Transform gizmo without sealing a Command
- **THEN** the Player world is unchanged by Play authorship patch
