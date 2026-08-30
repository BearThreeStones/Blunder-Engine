# play-authorship-patch Specification

## Purpose
Applies sealed Live Editor Commands onto the running Play Process world by Authorship Address, without Save and without Play Reload.
## Requirements
### Requirement: Patch is a sealed Command on the Play entry Live document
While a Play session is running, the editor SHALL send a Play authorship patch when a Document-History-sealed Editor Command targets the Live document that is this session’s Play entry scene (same Scene Asset GUID). The editor SHALL NOT require Save and SHALL NOT run Play Reload to apply that Command. Commands whose Live document is not that Play entry SHALL stay editor-only: no control-channel message and no Issue.

#### Scenario: Seal on Play entry patches
- **WHEN** a Play session is Playing or Paused and the author seals a v1-patchable Command on the Live document that is this session’s Play entry
- **THEN** the editor sends a Play authorship patch on the Play control channel
- **AND** the editor does not Save and does not Play Reload for that seal

#### Scenario: Other Live document is editor-only
- **WHEN** a Play session is running and the author seals a Command on a Live document that is not this session’s Play entry scene
- **THEN** the editor does not send a Play authorship patch
- **AND** no Issue is reported for skipping that Command

### Requirement: Authorship Address is the patch key
A Play authorship patch SHALL address the Play Process entity by Authorship Address (the scene-unique entity name). It SHALL NOT use EntityId or ObjectId as the product key.

#### Scenario: Name locates the Player entity
- **WHEN** a v1 patch is sent for Authorship Address `Hero`
- **THEN** the Player applies that patch to the entity named `Hero` in the Play Process world

### Requirement: v1 patch catalog is authored data on existing entities
v1 Play authorship patches SHALL cover authored data on entities that already exist in both the Live document and the Play Process world: Local Transform, Unique attachments, MeshRenderer, Behaviour bags, SkeletonModifiers, and animation-host fields. Spawn, delete, rename, reparent, and Global Commands SHALL NOT be v1 patches. Those Commands SHALL remain editor-only until Play Reload or a later Play. Uncommitted gizmo or field samples SHALL NOT become patches.

#### Scenario: Transform seal patches
- **WHEN** the author seals a Local Transform Command on a Play-entry entity that exists in the Player
- **THEN** the Player writes that Local Transform onto that entity

#### Scenario: Spawn stays editor-only
- **WHEN** the author seals a Spawn Command on the Play entry Live document while Playing
- **THEN** the editor does not send a v1 Play authorship patch for that Spawn
- **AND** the Play Process world does not gain that entity until Play Reload or a later Play

#### Scenario: Uncommitted gizmo sample is not a patch
- **WHEN** the author drags a Transform gizmo without sealing a Command
- **THEN** the Player world is unchanged by Play authorship patch

### Requirement: Patch writes at seal whether Playing or Paused
The Player SHALL apply a received v1 patch immediately whether the session is Playing or Paused. Later Behaviour Tick MAY overwrite the same fields. The system SHALL NOT lock patched properties against Tick until Reload.

#### Scenario: Patch while Playing
- **WHEN** a v1 patch arrives while Playing
- **THEN** the Player writes the authored data before the next Tick that may overwrite those fields

#### Scenario: Patch while Paused
- **WHEN** a v1 patch arrives while Paused
- **THEN** the Player writes the authored data
- **AND** Tick remains skipped until Resume or Play step

### Requirement: Undo Redo and History Jump also patch
Undo, Redo, and History Jump of a v1-patchable Command on the Play entry Live document SHALL send Play authorship patches for the resulting authored data. A History change that is not a v1-patchable Command SHALL NOT send a v1 patch.

#### Scenario: Undo patches the restored values
- **WHEN** the author Undoes a v1-patchable Transform Command on the Play entry while Playing
- **THEN** the Player receives a patch that restores the pre-Command Local Transform

#### Scenario: Jump of a non-v1 Command does not patch
- **WHEN** History Jump lands on a Spawn Command while Playing
- **THEN** the editor does not send a v1 Play authorship patch for that Spawn

### Requirement: Unknown address is Warning not History rollback
When a v1 patch names an Authorship Address that does not exist in the Play Process world, the Player SHALL skip that write and the editor SHALL report a Warning-grade Issue with code `play.patch_unknown_address`. The Document History seal SHALL remain. The editor SHALL NOT treat that miss as a Request failure, SHALL NOT roll back History, and SHALL NOT Error Pause for that Issue.

#### Scenario: Missing Player entity warns
- **WHEN** a v1 patch is sent for an Authorship Address the Play Process world does not contain
- **THEN** the Player does not write that patch
- **AND** a Warning Issue with code `play.patch_unknown_address` is reported
- **AND** Document History still contains the sealed Command
- **AND** Error Pause does not pause the Player for that Issue

